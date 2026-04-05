#include <boost/asio.hpp>
#include <set>
#include <iostream>
#include <memory>

using boost::asio::ip::tcp;
const int MAX_LENGTH = 1024;
typedef std::shared_ptr<tcp::socket> socket_ptr;
std::set<std::shared_ptr<std::thread>> thread_set;
using namespace std;

// 处理一个客户端的收发逻辑
void session(socket_ptr sock){
  try
  {
   for (;;){

    char data[MAX_LENGTH];
    memset(data, '\0', MAX_LENGTH);
    boost::system::error_code error;

    // size_t length = boost::asio::read(sock, boost::asio::buffer(data, MAX_LENGTH), error);
    size_t length = sock->read_some(boost::asio::buffer(data, MAX_LENGTH), error);
    
    
    if (error == boost::asio::error::eof){
      std::cout << "connection closed by peer" << std::endl;
      break;
    }else if (error){
      throw boost::system::system_error(error);
    }

    std::cout << "receive from " << sock -> remote_endpoint().address().to_string() << std::endl;
    std::cout << "receive msg: " << data << std::endl;

    // 回传
    boost::asio::write(*sock, boost::asio::buffer(data, length));
  }
  }
  catch (std::exception &e)
  {
    std::cerr << e.what() << '\n';
  }
}


void server(boost::asio::io_context &ioc, unsigned short port){
  tcp::acceptor a(ioc, tcp::endpoint(tcp::v4(), port));
  for (;;){
    socket_ptr socket(new tcp::socket(ioc));
    a.accept(*socket);
    auto t = std::make_shared<std::thread>(session, socket);
    thread_set.insert(t); // 放入线程集合
  }
}


int main()
{
  try
  {
    boost::asio::io_context ioc;
    server(ioc, 8080);
    for (auto &t: thread_set){
      t->join();
    }
  }
  catch(const std::exception& e)
  {
    std::cerr << e.what() << '\n';
  }
  
  return 0;
}