#include "http_conn.h"

// 模块职责与连接生命周期说明：
// - http_conn 对象代表一个客户端连接，包含读写缓冲、解析状态机、与文件映射状态。
// - 生命周期概述：init(sockfd) -> 主线程触发 EPOLLIN -> http_conn::read() 读取所有可读数据 -> 将对象交给线程池处理(process) ->
//   线程池线程中执行 process_read()（主状态机 + 从状态机解析请求），若得到完整请求则 process_write() 填充响应并设置 m_iv，
//   最终通过 modifyfd 将该 fd 的 epoll 事件改为 EPOLLOUT 触发主线程写出；写完后根据 Connection 决定是否 keep-alive 或关闭连接。
// - epoll 与 EPOLLONESHOT：每次将 fd 注册为 EPOLLONESHOT，处理线程在完成一次读取/写入后必须调用 modifyfd 恢复事件，
//   以保证不会有多个线程同时处理同一 fd（避免并发竞态）。
//
// 解析/响应关键点：
// - 主状态机：CHECK_STATE_REQUESTLINE -> CHECK_STATE_HEADER -> CHECK_STATE_CONTENT；parse_line() 用于按 CRLF 分割行。
// - 对静态文件请求：do_request 使用 stat 检查并用 mmap 映射文件到内存，response 通过 writev 发送头部与映射区两段内存。
// - 非阻塞读写：read() 循环 recv 直到返回 EAGAIN/EWOULDBLOCK；write() 使用 writev，在 EAGAIN 时修改为 EPOLLOUT 等待下一次可写。

int http_conn :: m_epollfd = -1; // 所有socket上事件注册到同一个epoll
int http_conn :: m_user_count = 0; //统计用户数量

// 定义HTTP响应的一些状态信息
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

const char* doc_root = "./resources";

// 设置文件描述符非阻塞
int setnonblocking(int fd){

    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
    return old_flag;
}


// 添加文件描述符到epoll中
void addfd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    // 此处修改触发模式
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; // ET
    //event.events = EPOLLIN | EPOLLRDHUP; // LT

    if (one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置文件描述符非阻塞
    setnonblocking(fd);
}
// 从epoll中删除文件描述符
void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改epoll文件描述符, 重置socket上EPOLLONESHOT
// 确保下次可读时，EPOLLONESHOT可触发
void modifyfd(int epollfd, int fd ,int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP | EPOLLET ; // 同步修改触发模式
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 客户端连接初始化
void http_conn:: init(int sockfd, const sockaddr_in &addr){

    m_sockfd = sockfd;
    m_address = addr;

    // 设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll事件组中
    addfd(m_epollfd, m_sockfd, true); 
    
    
    m_user_count++;
    init();

}

// 连接解析状态初始化
void http_conn :: init(){

    m_check_state = CHECK_STATE_REQUESTLINE;    // 初始状态为检查请求行
    m_linger = false;       // 默认不保持链接  Connection : keep-alive保持连接

    m_method = GET;         // 默认请求方式为GET
    m_url = 0;              
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_index = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    bzero(m_read_buf, READ_BUFFER_SIZE);
    bzero(m_write_buf, READ_BUFFER_SIZE);
    bzero(m_real_file, FILENAME_LEN);

}

// 关闭连接
void http_conn :: closeconn(){
    if (m_sockfd != -1){
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--; // 关闭一个连接，客户总数量减1
    }
}


/// 非阻塞读, 直到无数据可读或者对方关闭连接
bool  http_conn ::read(){
    if (m_read_idx >= READ_BUFFER_SIZE) return false;

    // 读取到的字节
    int bytes_read = 0;

    while (true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 没有数据
                break;
            }  
            return false;  
        }else if (bytes_read == 0) return false;
        
        m_read_idx += bytes_read;
    }
    //printf("read: %s\n", m_read_buf);
    return true;
}

// 写HTTP响应
bool http_conn::write()
{
    int temp = 0;
    int bytes_have_send = 0;    // 已经发送的字节
    int bytes_to_send = m_write_idx;// 将要发送的字节 （m_write_idx）写缓冲区中待发送的字节数
    
    if ( bytes_to_send == 0 ) {
        // 将要发送的字节为0，这一次响应结束。
        modifyfd( m_epollfd, m_sockfd, EPOLLIN ); 
        init();
        return true;
    }

    while(1) {
        // 分散写
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if ( temp <= -1 ) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if( errno == EAGAIN ) {
                modifyfd( m_epollfd, m_sockfd, EPOLLOUT );
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        bytes_have_send += temp;
        if ( bytes_to_send <= bytes_have_send ) {
            // 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接
            unmap();
            if(m_linger) {
                init();
                modifyfd( m_epollfd, m_sockfd, EPOLLIN );
                return true;
            } else {
                modifyfd( m_epollfd, m_sockfd, EPOLLIN );
                return false;
            } 
        }
    }
}


// 线程池中工作线程调用，处理HTTP请求的入口函数
void http_conn :: process(){
    // 解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST){
        modifyfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }

    // 生成响应
    bool write_ret = process_write( read_ret );
    if ( !write_ret ) {
        closeconn();
    }
    modifyfd( m_epollfd, m_sockfd, EPOLLOUT);
}

/*
在类外部定义函数时，需要明确指定枚举值的作用域

GET_REQUEST 是 http_conn::HTTP_CODE 枚举的一个值
*/


// 主状态机
http_conn :: HTTP_CODE  http_conn :: process_read(){

    LINE_STATUS  line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) 
    || (line_status = parse_line()) == LINE_OK){
        // 解析到了一行完整的数据 或 解析到请求体，也是完整的数据

        // 获取一行数据
        text = get_line();

        m_start_line = m_checked_index;
        printf("got 1 http line:%s\n", text);


        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            
        case CHECK_STATE_HEADER:
            /* code */
            {
                ret = parse_headers(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }else if (ret == GET_REQUEST){
                    return do_request(); // 解析具体信息
                }
                break;
            }
        case CHECK_STATE_CONTENT:
            /* code */
            {
                ret = parse_content(text);
                if (ret == GET_REQUEST){
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
        
        default:
            {
            return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}
/// @brief  解析请求行, 获取请求方法，目标的URL，HTTP版本
http_conn :: HTTP_CODE http_conn :: parse_request_line(char *text){
    // GET /index.html HTTP/1.1
    m_url = strpbrk(text , " \t");
    if (!m_url){
        return BAD_REQUEST;
    }

    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';  // 先用原值赋\0， 后++， 

    // GET\0  字符串结束符截断
    char* method = text;
    if (strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else {
        return BAD_REQUEST;
    }


    // /index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if (!m_version){
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    // http://192.168.1.1:10000/index.html
    if (strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7; //  192.168.1.1:10000/index.html
        m_url = strchr(m_url, '/'); //  /index.html
    }

    if (!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER; // 改变主状态机状态

    return NO_REQUEST;

}
/// @brief  解析头部信息
http_conn :: HTTP_CODE http_conn :: parse_headers(char *text){
    
    // 遇到空行，表示头部字段解析完毕
    if( text[0] == '\0' ) {
        // 如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，
        // 状态机转移到CHECK_STATE_CONTENT状态
        if ( m_content_length != 0 ) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        // 否则说明已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } else if ( strncasecmp( text, "Connection:", 11 ) == 0 ) {
        // 处理Connection 头部字段  Connection: keep-alive
        text += 11;
        text += strspn( text, " \t" );
        if ( strcasecmp( text, "keep-alive" ) == 0 ) {
            m_linger = true;
        }
    } else if ( strncasecmp( text, "Content-Length:", 15 ) == 0 ) {
        // 处理Content-Length头部字段
        text += 15;
        text += strspn( text, " \t" );
        m_content_length = atol(text);
    } else if ( strncasecmp( text, "Host:", 5 ) == 0 ) {
        // 处理Host头部字段
        text += 5;
        text += strspn( text, " \t" );
        m_host = text;
    } else {
        printf( "oops! unknow header %s\n", text );
    }
    return NO_REQUEST;

}
/// @brief  解析请求体
// 没有真正解析HTTP请求的消息体，只是判断它是否被完整的读入了
http_conn::HTTP_CODE http_conn::parse_content( char* text ) {
    if ( m_read_idx >= ( m_content_length + m_checked_index ) )
    {
        text[ m_content_length ] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
/// @brief  解析一行数据, 判断依据为\r\n
http_conn :: LINE_STATUS http_conn :: parse_line(){
    
    char temp;
    for (; m_checked_index < m_read_idx; ++ m_checked_index){
        temp = m_read_buf[m_checked_index];
        if(temp == '\r'){
            if((m_checked_index + 1) == m_read_idx) return LINE_OPEN;
            else if (m_read_buf[m_checked_index + 1]== '\n') {
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if (temp == '\n'){
            if((m_checked_index > 1) && (m_read_buf[m_checked_index-1] == '\r')){
                m_read_buf[m_checked_index-1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    
    return LINE_OPEN;
    
}
// 当得到一个完整、正确的HTTP请求时，我们就分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功
http_conn::HTTP_CODE http_conn::do_request()
{
    // "/home/miao/TinyWebserver/resources"
    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
    // 获取m_real_file文件的相关的状态信息，-1失败，0成功
    if ( stat( m_real_file, &m_file_stat ) < 0 ) {
        return NO_RESOURCE;
    }

    // 判断访问权限
    if ( ! ( m_file_stat.st_mode & S_IROTH ) ) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录
    if ( S_ISDIR( m_file_stat.st_mode ) ) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件
    int fd = open( m_real_file, O_RDONLY );
    // 创建内存映射
    m_file_address = ( char* )mmap( 0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    return FILE_REQUEST;
}
// 对内存映射区执行munmap操作
void http_conn::unmap() {
    if( m_file_address )
    {
        munmap( m_file_address, m_file_stat.st_size );
        m_file_address = 0;
    }
}






// 往写缓冲中写入待发送的数据
bool http_conn::add_response( const char* format, ... ) {
    if( m_write_idx >= WRITE_BUFFER_SIZE ) {
        return false;
    }
    va_list arg_list;
    va_start( arg_list, format );
    int len = vsnprintf( m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list );
    if( len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx ) ) {
        return false;
    }
    m_write_idx += len;
    va_end( arg_list );
    return true;
}

bool http_conn::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title );
}

bool http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

bool http_conn::add_content_length(int content_len) {
    return add_response( "Content-Length: %d\r\n", content_len );
}

bool http_conn::add_linger()
{
    return add_response( "Connection: %s\r\n", ( m_linger == true ) ? "keep-alive" : "close" );
}

bool http_conn::add_blank_line()
{
    return add_response( "%s", "\r\n" );
}

bool http_conn::add_content( const char* content )
{
    return add_response( "%s", content );
}

bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 根据服务器处理HTTP请求的结果，决定返回给客户端的内容
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret)
    {
        case INTERNAL_ERROR:
            add_status_line( 500, error_500_title );
            add_headers( strlen( error_500_form ) );
            if ( ! add_content( error_500_form ) ) {
                return false;
            }
            break;
        case BAD_REQUEST:
            add_status_line( 400, error_400_title );
            add_headers( strlen( error_400_form ) );
            if ( ! add_content( error_400_form ) ) {
                return false;
            }
            break;
        case NO_RESOURCE:
            add_status_line( 404, error_404_title );
            add_headers( strlen( error_404_form ) );
            if ( ! add_content( error_404_form ) ) {
                return false;
            }
            break;
        case FORBIDDEN_REQUEST:
            add_status_line( 403, error_403_title );
            add_headers(strlen( error_403_form));
            if ( ! add_content( error_403_form ) ) {
                return false;
            }
            break;
        case FILE_REQUEST:
            add_status_line(200, ok_200_title );
            add_headers(m_file_stat.st_size);
            m_iv[ 0 ].iov_base = m_write_buf;
            m_iv[ 0 ].iov_len = m_write_idx;
            m_iv[ 1 ].iov_base = m_file_address;
            m_iv[ 1 ].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            return true;
        default:
            return false;
    }

    m_iv[ 0 ].iov_base = m_write_buf;
    m_iv[ 0 ].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}
