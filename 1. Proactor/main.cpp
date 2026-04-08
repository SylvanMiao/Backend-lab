#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <signal.h>

#include "threadpool.h"
#include "locker.h"
#include "http_conn.h"

// 服务器总体工作逻辑说明：
// 1. 主线程完成初始化：忽略 SIGPIPE、创建线程池、分配 clients 数组、创建监听 socket 并加入 epoll。
// 2. 使用 epoll_wait 循环等待事件：
//    - 如果发生监听 socket 的可读事件，accept 新连接，设置 socket 非阻塞，注册到 epoll（使用 EPOLLONESHOT + ET）并把连接对象初始化到 users[]。
//    - 对客户端 fd 的 EPOLLIN 事件：主线程调用 http_conn::read() 非阻塞读入全部数据，若读完则将该 http_conn 对象放入线程池队列处理（pool->append）。
//    - 线程池中工作线程取出任务后调用 http_conn::process()：解析请求（状态机 parse），生成响应（process_write），并通过修改 epoll 事件触发写事件。
//    - 对客户端 fd 的 EPOLLOUT 事件：主线程调用 http_conn::write()，使用 writev 发送响应（包含头部缓冲区和 mmap 的文件），完成后根据 Connection 决定是否继续保持连接或关闭。
// 3. 关键点说明：
//    - 使用 EPOLLONESHOT 保证同一连接不会被多个线程并发处理，处理完成后需手动通过 modifyfd 恢复 EPOLLONESHOT 以再次接收事件。
//    - 使用非阻塞 I/O + 边沿触发（EPOLLET），read() 需要循环读取直到 EAGAIN/EWOULDBLOCK。
//    - 使用 mmap 将文件映射到内存并通过 writev 发送，发送完成需 munmap。
//    - 线程池负责耗时的请求解析与响应准备，主线程负责 I/O 事件分发与短小的非阻塞读写触发调度。

#define MAX_FD 65535 //最大文件描述符个数
#define MAX_EVENT_NUM 10000 //一次最大监听事件个数

// 添加信号捕捉
void addsig(int sig,  void(handler)(int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}



// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
// 修改文件描述符
extern void modifyfd(int epollfd, int fd ,int ev);

int main(int argc, char* argv[]){
    
    if (argc <= 1){
        printf("按照如下格式运行： %s port num needed\n", basename(argv[0]));
        exit(-1);
    }

    int port = atoi(argv[1]);

    // 对SIGPIE处理
    addsig(SIGPIPE, SIG_IGN);

    // 初始化线程池
    threadpool<http_conn>* pool = NULL;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch(const std::exception & e)
    {
        std::cerr << e.what() << std::endl;
        exit(-1);
    }
    
    // 创建数组用于保存所有客户端信息
    http_conn *users = new http_conn[MAX_FD];

    // 创建套接字
    // IPv4 TCP
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1){
        perror("socket create");
        exit(-1);
    }

    // 设置端口复用
    int reuse = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in address;
    address.sin_port = htons(port);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    // bind
    int ret = bind(lfd, (struct sockaddr*) &address, sizeof(address));
    if (ret == -1){
        perror("bind");
        exit(-1);
    }

    // 监听
    ret = listen(lfd, 5);
    if (ret == -1){
        perror("listen");
        exit(-1);
    }

    // 创建epoll对象，事件数组，添加监听事件
    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);

    addfd(epollfd, lfd, false);
    http_conn::m_epollfd = epollfd;

    // 主线程检测
    while(true){
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if( (num < 0) && (errno != EINTR)){
            printf("epoll failed\n");
            break;
        }

        // 循环遍历事件数组
        for (int i = 0; i < num; i++){
        
            int sockfd = events[i].data.fd;
            // 有客户端连接情况
            if (sockfd == lfd){
                struct sockaddr_in clientaddr;
                socklen_t len = sizeof(clientaddr);
                int connfd = accept(lfd, (struct sockaddr*)&clientaddr, &len);

                if (http_conn::m_user_count >= MAX_FD){
                    // 目前连接数已满
                    // 给客户端写一个信息：服务器正忙
                    close(connfd);
                    continue;
                }

                // 将connfd客户数据初始化后放入用户信息数组
                users[connfd].init(connfd, clientaddr);
            
            }

            // 有客户端错误事件情况
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP| EPOLLERR))
            {
                users[sockfd].closeconn();
            }
            // 有客户端读事件情况
            else if (events[i].events & EPOLLIN){
                if (users[sockfd].read()){
                    // 一次性把所有数据读完
                    pool->append(users + sockfd);
                }else {
                    users[sockfd].closeconn();
                }
            }
            
            else if (events[i].events & EPOLLOUT){
                if (users[sockfd].write()){
                    // 一次性把所有数据写完
                }else {
                    users[sockfd].closeconn();
                }
            }
        }

    }
    close(epollfd);
    close(lfd);
    delete[] users;
    delete pool;

return 0;
}