#include "apiexample.h"
#include <boost/asio.hpp>
#include <iostream>

using namespace boost;

/**
 * @brief 客户端端点示例：解析字符串 IP 并构造 tcp::endpoint
 *
 * 解析 raw_ip_address 为 asio::ip::address 并打印到标准输出。
 * 如果解析失败，会输出错误信息并返回对应的错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示 boost::system::error_code 的值
 * @see accept_new_connection()  此函数为服务端完整流程的一个部分（演示端点构造）
 */
int client_end_point()
{
  std::string raw_ip_address = "127.0.0.1";
  unsigned short port_num = 3333;

  boost::system::error_code ec;
  asio::ip::address ip_address = asio::ip::address::from_string(raw_ip_address, ec);
  std::cout << ip_address << std::endl;

  if (ec.value() != 0)
  {
    std::cout << "Failed to parse the IP address. Error code ="
              << ec.value() << ". Message is" << ec.message();
    return ec.value();
  }

  asio::ip::tcp::endpoint ep(ip_address, port_num);
  return 0;
}

/**
 * @brief 服务端端点示例：构造监听任意 IPv6 地址的 tcp::endpoint
 *
 * 使用 address_v6::any() 构造一个绑定任意 IPv6 地址的端点并返回。
 *
 * @return int 返回 0 表示成功
 * @see accept_new_connection()  此函数为服务端完整流程的一个部分（演示端点构造）
 */
int server_end_point()
{

  unsigned short port_num = 3333;
  asio::ip::address ip_address = asio::ip::address_v6::any();
  asio::ip::tcp::endpoint ep(ip_address, port_num);

  return 0;
}

/**
 * @brief 演示如何创建 TCP socket 并打开指定协议
 *
 * 创建 io_context、构造 tcp::socket 并用指定协议 open()。
 * 若打开失败，会打印错误信息并返回错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示 boost::system::error_code 的值
 * @see accept_new_connection()  此函数展示了 accept_new_connection() 中 socket 创建/打开 的相关步骤
 */
int create_tcp_socket()
{
  // 上下文
  asio::io_context ioc;
  // 协议
  asio::ip::tcp protocol = asio::ip::tcp::v4();
  // socket
  asio::ip::tcp::socket soc(ioc);

  boost::system::error_code ec;
  soc.open(protocol, ec);
  if (ec.value() != 0)
  {
    std::cout << "Failed to parse the IP address. Error code ="
              << ec.value() << ". Message is" << ec.message();
    return ec.value();
  }
  return 0;
}

/**
 * @brief 演示如何创建 TCP acceptor 并打开指定协议
 *
 * 使用 acceptor.open(protocol) 打开 acceptor，若失败打印错误并返回错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示 boost::system::error_code 的值
 * @see accept_new_connection()  此函数展示了 acceptor 创建/打开 的相关步骤
 */
int create_acceptor_socket()
{
  asio::io_context ios;
  asio::ip::tcp::acceptor acceptor(ios);

  // 协议
  asio::ip::tcp protocol = asio::ip::tcp::v4();

  boost::system::error_code ec;
  acceptor.open(protocol, ec);
  if (ec.value() != 0)
  {
    std::cout << "Failed to parse the IP address. Error code ="
              << ec.value() << ". Message is" << ec.message();
    return ec.value();
  }

  // 现代做法
  // asio::ip::tcp::acceptor a(ios, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 333));
  return 0;
}

/**
 * @brief 将 acceptor 绑定到指定端口
 *
 * 构造 endpoint 并调用 acceptor.bind(ep) 进行绑定，若失败打印错误并返回错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示 boost::system::error_code 的值
 * @see accept_new_connection()  此函数对应 accept_new_connection() 中的 bind 步骤
 */
int bind_acceptor_socket()
{
  unsigned short port = 3333;
  asio::ip::tcp::endpoint ep(asio::ip::address_v4::any(), port);

  asio::io_context ios;
  asio::ip::tcp::acceptor acceptor(ios, ep.protocol());

  boost::system::error_code ec;
  acceptor.bind(ep, ec);
  if (ec.value() != 0)
  {
    std::cout << "Failed to parse the IP address. Error code ="
              << ec.value() << ". Message is" << ec.message();
    return ec.value();
  }
  return 0;
}

/**
 * @brief 连接到指定 IP:port 的示例（同步 connect）
 *
 * 使用 asio::ip::address::from_string 创建 endpoint，并构造 socket 后调用 connect。
 * 捕获 system::system_error 并在错误时打印信息和返回错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示异常中捕获的错误码
 * @see accept_new_connection()  与服务端流程互为对应（客户端连接到服务端）
 */
int connect_to_end()
{
  std::string raw_ip_address = "192.168.1.124";
  unsigned short port_num = 3333;

  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ios;
    // 对端支持的协议
    asio::ip::tcp::socket sock(ios, ep.protocol());
    sock.connect(ep);
  }
  catch (system::system_error &er)
  {
    std::cout << "Failed to parse the IP address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}

/**
 * @brief 通过 DNS 解析主机名并连接（使用 resolver）
 *
 * 使用 tcp::resolver 解析主机名和端口，得到 iterator 后调用 asio::connect 连接。
 * 捕获 system::system_error 并在错误时输出信息和返回错误码。
 *
 * @return int 返回 0 表示成功，非 0 表示异常中捕获的错误码
 * @see accept_new_connection()  与服务端 accept 流程配合使用
 */
int dns_connect_end()
{
  std::string host = "www.baidu.com";
  std::string port_num = "3333";
  asio::io_context ios;
  asio::ip::tcp::resolver::query resolver_query(host, port_num, asio::ip::tcp::resolver::query::numeric_service);
  asio::ip::tcp::resolver resolver(ios);

  try
  {
    asio::ip::tcp::resolver::iterator it = resolver.resolve(resolver_query);
    asio::ip::tcp::socket sock(ios);
    asio::connect(sock, it);
  }
  catch (system::system_error &er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}

/**
 * @brief 接受一个新连接的示例（同步 accept） — 服务端完整流程（核心）
 *
 * 该函数展示了服务端的完整同步流程：创建 acceptor、bind 到端点、listen，随后 accept 一个连接。
 * 也就是说，其他函数（如 bind_acceptor_socket、create_acceptor_socket、create_tcp_socket 等）
 * 分别演示了该流程中的各个组成步骤。将这些步骤组合起来即可得到本函数的实现。
 *
 * @return int 返回 0 表示成功，非 0 表示异常中捕获的错误码
 */
int accept_new_connection()
{
  const int BACKLOG_SIZE = 30;
  unsigned short port_num = 3333;

  asio::ip::tcp::endpoint ep(asio::ip::address_v4::any(), port_num);
  asio::io_context ios;
  try
  {
    asio::ip::tcp::acceptor acceptor(ios, ep.protocol());
    acceptor.bind(ep);
    acceptor.listen(BACKLOG_SIZE);
    asio::ip::tcp::socket sock(ios);
    acceptor.accept(sock);
  }
  catch (system::system_error &er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}



// 传统写法
/**
 * @brief 演示使用 asio::const_buffer 持有只读字符串数据的方式。
 *
 * 该函数展示如何将 std::string 的数据包装为 asio::const_buffer，
 * 并把它放入一个 buffer sequence（vector）。
 */
void use_const_buffer()
{
  std::string buf = "hello world";
  asio::const_buffer asio_buf(buf.c_str(), buf.length());
  std::vector<asio::const_buffer> buffer_sequence;
  buffer_sequence.push_back(asio_buf);
}


// buffer()用法
/**
 * @brief 演示使用 asio::buffer 直接创建 const_buffers_1。
 *
 * 返回的 buffer 类型可直接用于 asio 的读写接口（只读视图）。
 */
void use_buffer_str()
{
  asio::const_buffers_1 output_buf = asio::buffer("hello world");
}

// buffer()数组类型转换
/**
 * @brief 演示如何把动态分配的原始内存转换为 asio::mutable_buffer。
 *
 * 该示例使用 unique_ptr 管理内存，并将其转换为 void* 指针传给 asio::buffer。
 */
void use_buffer_array()
{
  const size_t BUF_SIZE_BYTES = 20;
  std::unique_ptr<char> buf(new char[BUF_SIZE_BYTES]);
  auto input_buf = asio::buffer(static_cast<void*>(buf.get()), BUF_SIZE_BYTES);
}

/**
 * @brief 将给定 socket 同步写入完整字符串（循环调用 write_some）。
 *
 * @param sock 目标 TCP socket（引用），必须已连接。
 */
void write_to_socket(asio::ip::tcp::socket &sock){
  std::string buf = "hello world";
  std::size_t total_bytes_written = 0;
  // 循环发送
  // write_some 返回每次写入的字节数，受到TCP内核发送缓冲区的影响，所有用户态和tcp可能不统一
  while (total_bytes_written != buf.length()){
    total_bytes_written += sock.write_some(asio::buffer(buf.c_str()+total_bytes_written, 
          buf.length() - total_bytes_written)); // 前一个参数：每次写的首地址； 后一个参数：剩余的长度
  }
}

/**
 * @brief 同步通过 socket 发数据（使用 write_some 循环）。
 *
 * 建立到给定 IP:port 的连接后调用 write_to_socket 发送数据。
 *
 * @return 0 表示成功；非 0 表示异常时的错误码。
 */
int send_data_by_write_some(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    // asio::io_service ios; 老版本
    asio::io_context ioc; // 通讯所需的上下文
    asio::ip::tcp::socket sock(ioc, ep.protocol()); // 创建socket
    sock.connect(ep);
    write_to_socket(sock);
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}

/**
 * @brief 同步通过 socket 发数据（使用 socket.send 单次阻塞发送）。
 *
 * 该函数在连接建立后调用 sock.send，返回发送的字节数。
 *
 * @return 0 表示成功或未发送；非 0 表示异常时的错误码。
 */
int send_data_by_send(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  std::string buf = "hello world";
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ioc; 
    asio::ip::tcp::socket sock(ioc, ep.protocol()); 
    sock.connect(ep);
    int sendlength = sock.send(asio::buffer(buf.c_str(), buf.length()));
    if (sendlength <= 0){
      return 0;
    }
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}


/**
 * @brief 同步通过 asio::write 将整个缓冲区阻塞写完。
 *
 * 使用 asio::write 保证写入直到缓冲区全部发送或发生错误。
 *
 * @return 0 表示成功或未发送；非 0 表示异常时的错误码。
 */
int send_data_by_write(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  std::string buf = "hello world";
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ioc; 
    asio::ip::tcp::socket sock(ioc, ep.protocol()); 
    sock.connect(ep);
    int sendlength = asio::write(sock, asio::buffer(buf.c_str(), buf.length()));
    if (sendlength <= 0){
      return 0;
    }
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
  return 0;
}



/**
 * @brief 从 socket 同步读取固定长度的数据并返回为 std::string。
 *
 * @param sock 已连接的 TCP socket 引用。
 * @return 读取到的字节构成的 std::string（长度等于 MESSAGE_SIZE）。
 */
std::string read_from_socket(asio::ip::tcp::socket &sock){
  const unsigned char MESSAGE_SIZE = 7;
  char buf[MESSAGE_SIZE];
  std::size_t total_bytes_read = 0;
  while (total_bytes_read != MESSAGE_SIZE){
    total_bytes_read += sock.read_some(asio::buffer(buf + total_bytes_read, 
      MESSAGE_SIZE - total_bytes_read));
  }
  return std::string(buf, total_bytes_read);
}

/**
 * @brief 使用 read_some 循环从远端读取固定长度的数据（同步示例）。
 *
 * 建立到目标 IP:port 的连接后，调用 read_from_socket 读取数据。
 *
 * @return 0 表示成功；非 0 表示异常时的错误码。
 */
int read_data_by_read_some(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ioc;
    asio::ip::tcp::socket sock(ioc, ep.protocol());
    sock.connect(ep);
    std::string result = read_from_socket(sock);
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
}


/**
 * @brief 使用 socket.receive 同步接收数据（一次 receive 调用）。
 *
 * 该函数演示使用 socket.receive 读取固定大小缓冲区并检查返回值。
 *
 * @return 0 表示成功或接收失败（长度<=0）；非 0 表示异常时的错误码。
 */
int read_data_by_receive(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  const unsigned char MESSAGE_SIZE = 7;
  char buf[MESSAGE_SIZE];
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ioc;
    asio::ip::tcp::socket sock(ioc, ep.protocol());
    sock.connect(ep);
    int receive_length = sock.receive(asio::buffer(buf, MESSAGE_SIZE));
    if (receive_length <= 0){
      std::cout << "Receive failed" << std::endl;
      return 0;
    }
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
}


/**
 * @brief 使用 asio::read 同步读取指定字节数（阻塞直到填满或错误）。
 *
 * asio::read 会持续读取直到缓冲区填满或发生错误，适合读取固定长度消息。
 *
 * @return 0 表示成功或接收失败（长度<=0）；非 0 表示异常时的错误码。
 */
int read_data_by_read(){
  std::string raw_ip_address = "192.168.3.11";
  unsigned short port_num = 3333;
  const unsigned char MESSAGE_SIZE = 7;
  char buf[MESSAGE_SIZE];
  try
  {
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string(raw_ip_address), port_num);
    asio::io_context ioc;
    asio::ip::tcp::socket sock(ioc, ep.protocol());
    sock.connect(ep);
    int receive_length = asio::read(sock, asio::buffer(buf, MESSAGE_SIZE));
    if (receive_length <= 0){
      std::cout << "Receive failed" << std::endl;
      return 0;
    }
  }
  catch(system::system_error& er)
  {
    std::cout << "Failed to parse the DNS address. Error code ="
              << er.code().value() << ". Message is" << er.what();
    return er.code().value();
  }
}
