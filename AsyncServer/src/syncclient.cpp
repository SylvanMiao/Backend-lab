#include <boost/asio.hpp>
#include <iostream>
#include <cstdint>
using namespace boost::asio::ip;
using namespace std;
const int MAX_LENGTH = 1024;
const int HEAD_LENGTH = 2;
int main()
{
  try {
    //创建上下文服务
    boost::asio::io_context   ioc;
    //构造endpoint
    tcp::endpoint  remote_ep(address::from_string("127.0.0.1"), 8080);
    tcp::socket  sock(ioc);
    boost::system::error_code   error = boost::asio::error::host_not_found; ;
    sock.connect(remote_ep, error);
    if (error) {
        cout << "connect failed, code is " << error.value() << " error msg is " << error.message();
        return 0;
    }
    std::cout << "Enter message: ";
    char request[MAX_LENGTH];
    std::cin.getline(request, MAX_LENGTH);
    size_t request_length = strlen(request);
    if (request_length > 65535)
    {
      std::cout << "message too long" << std::endl;
      return 0;
    }
    std::uint16_t request_len = static_cast<std::uint16_t>(request_length);
    char send_data[MAX_LENGTH] = { 0 };
    std::uint16_t request_len_net = boost::asio::detail::socket_ops::host_to_network_short(request_len);
    memcpy(send_data, &request_len_net, HEAD_LENGTH);
    memcpy(send_data + 2, request, request_length);
    boost::asio::write(sock, boost::asio::buffer(send_data, request_length+2));
    char reply_head[HEAD_LENGTH];
    size_t reply_length = boost::asio::read(sock,boost::asio::buffer(reply_head, HEAD_LENGTH));
    std::uint16_t msglen_net = 0;
    memcpy(&msglen_net, reply_head, HEAD_LENGTH);
    std::uint16_t msglen = boost::asio::detail::socket_ops::network_to_host_short(msglen_net);
    char msg[MAX_LENGTH] = { 0 };
    size_t  msg_length = boost::asio::read(sock,boost::asio::buffer(msg, msglen));
    std::cout << "Reply is: ";
    std::cout.write(msg, msglen) << endl;
    std::cout << "Reply len is " << msglen;
    std::cout << "\n";
}
catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << endl;
}
return 0;

  return 0;
}