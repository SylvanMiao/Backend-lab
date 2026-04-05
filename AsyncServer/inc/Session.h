#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <iostream>
#include <queue>

using namespace boost;
using namespace std;
// 最大报文接收大小
const int RECVSIZE = 1024;

// 会话连接类
class Session
{
public:
  Session(std::shared_ptr<asio::ip::tcp::socket> socket);
  void Connect(const asio::ip::tcp::endpoint &ep);

  // 这两个函数在异步调用的时候可能有顺序问题
  void WriteCallBack(const boost::system::error_code &ec, std::size_t bytes_transfered,
                     std::shared_ptr<MsgNode>);
  void WriteToSocket(const std::string &buf);

  // 用队列的正确方法
  void WriteCallBackWithQueue(const boost::system::error_code &ec, std::size_t bytes_transfered);
  void WriteToSocketWithQueue(const std::string &buf);

  // 另一种api一次性写完
  void WriteAllToSocket(const std::string &buf);
  void WriteAllCallBack(const boost::system::error_code &ec, std::size_t bytes_transferred);


  // // 异步读操作
  void ReadCallBack(const boost::system::error_code &ec, std::size_t bytes_transfered);
  void ReadFromSocket();


  // 一次性读完
  void ReadAllFromSocket();
  void ReadAllCallBack(const boost::system::error_code& ec, std::size_t bytes_transferred);
  
private:
  std::queue<std::shared_ptr<MsgNode>> _send_queue;
  std::shared_ptr<asio::ip::tcp::socket> _socket;
  std::shared_ptr<MsgNode> _sendnode;
  bool _sendpending;

  std::shared_ptr<MsgNode> _recvnode;
  bool _recvpending;
};

// 收发节点
class MsgNode
{
public:
  // 作为发送节点时
  MsgNode(const char *msg, int total_len) : _total_len(total_len), _cur_len(0)
  {
    _msg = new char[total_len];
    memcpy(_msg, msg, total_len);
  }

  // 作为接受节点时
  MsgNode(int total_len) : _total_len(total_len), _cur_len(0)
  {
    _msg = new char[total_len];
  }
  ~MsgNode()
  {
    delete[] _msg;
  }
  // 消息首地址
  char *_msg;
  // 总长度
  int _total_len;
  // 当前长度
  int _cur_len;
};