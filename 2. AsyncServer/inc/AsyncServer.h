#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <map>
#include <queue>
#include <mutex>
#include <cstdint>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

using boost::asio::ip::tcp;
#define HEAD_SIZE 2
#define MAX_SENDQUEUE_SIZE 1000
/*
异步读写的echo服务器示例

用伪闭包延长生命周期
防止 callback 执行时对象被销毁

技术用的是智能指针
*/

class Server;
class Session;

class MsgNode
{
  friend class Session;

public:
  // 用于发送 
  MsgNode(char *msg, int max_len) : _total_len(max_len + HEAD_SIZE), _cur_len(0)
  {
    _data = new char[_total_len + 1];
    // 转成网络字节序
    std::uint16_t len_net = boost::asio::detail::socket_ops::host_to_network_short(static_cast<std::uint16_t>(max_len));
    memcpy(_data, &len_net, HEAD_SIZE);
    memcpy(_data + HEAD_SIZE, msg, max_len);
    _data[_total_len] = '\0';
  }
  // 用于接收
  MsgNode(int max_len) : _total_len(max_len), _cur_len(0)
  {
    _data = new char[_total_len + 1];
    memset(_data, 0, _total_len + 1);
  }
  ~MsgNode()
  {
    delete[] _data;
  }

  void Clear()
  {
    memset(_data, 0, _total_len);
    _cur_len = 0;
  }

private:
  int _cur_len;
  int _total_len;
  char *_data;
};

class Session : public std::enable_shared_from_this<Session>
{
private:
  tcp::socket _socket;
  enum
  {
    max_length = 1024 * 2
  };
  Server *_server;
  std::string _uuid;

  std::queue<std::shared_ptr<MsgNode>> _send_que;
  std::mutex _send_lock;

  // 收到的消息结构
  std::shared_ptr<MsgNode> _recv_msg_node;
  // 收到的头部结构
  std::shared_ptr<MsgNode> _recv_head_node;

public:
  Session(boost::asio::io_context &ioc, Server *server)
      : _socket(ioc), _server(server), _recv_head_node(std::make_shared<MsgNode>(HEAD_SIZE))
  {
    boost::uuids::uuid a_uuid = boost::uuids::random_generator()();
    _uuid = boost::uuids::to_string(a_uuid);
  }
  tcp::socket &Socket()
  {
    return _socket;
  }

  std::string &GetUuid();
  void Start();
  void Close();
  void handle_read_head(const boost::system::error_code &error, size_t bytes_transfered, std::shared_ptr<Session> _self_shared);
  void handle_read_body(const boost::system::error_code &error, size_t bytes_transfered, std::shared_ptr<Session> _self_shared);
  void handle_write(const boost::system::error_code &error, std::shared_ptr<Session> _self_shared);
  void Send(char *msg, int max_length);
};

class Server
{
public:
  Server(boost::asio::io_context &ioc, short port);
  void ClearSession(std::string _uuid);

private:
  void start_accept();
  void handle_accept(std::shared_ptr<Session>, const boost::system::error_code &error);

  boost::asio::io_context &_ioc;
  tcp::acceptor _acceptor;
  std::map<std::string, std::shared_ptr<Session>> _sessions;
};