#include "Session.h"

Session::Session(std::shared_ptr<asio::ip::tcp::socket> socket) : _socket(socket),
                                                                  _sendpending(false), _recvpending(false)
{
}

void Session::Connect(const asio::ip::tcp::endpoint &ep)
{
  _socket->connect(ep);
}

void Session::WriteCallBack(const boost::system::error_code &ec, std::size_t bytes_transfered, std::shared_ptr<MsgNode> msg_node)
{
  if (ec.value() != 0)
  {
    std::cout << "Error Code is" << ec.value() << " Message is " << ec.message();
    return;
  }
  // 检查未发送完的长度
  if ((bytes_transfered + msg_node->_cur_len) < msg_node->_total_len)
  {
    msg_node->_cur_len += bytes_transfered;
    this->_socket->async_write_some(asio::buffer(_sendnode->_msg + _sendnode->_cur_len, _sendnode->_total_len - _sendnode->_cur_len),
                                    std::bind(&Session::WriteCallBack, this, std::placeholders::_1, std::placeholders::_2,
                                              _sendnode));
  }
}

void Session::WriteToSocket(const std::string &buf)
{
  _sendnode = make_shared<MsgNode>(buf.c_str(), buf.length());
  this->_socket->async_write_some(asio::buffer(_sendnode->_msg, _sendnode->_total_len),
                                  std::bind(&Session::WriteCallBack, this, std::placeholders::_1, std::placeholders::_2,
                                            _sendnode));
}

void Session::WriteCallBackWithQueue(const boost::system::error_code &ec, std::size_t bytes_transfered)
{
  if (ec.value() != 0)
  {
    std::cout << "Error Code is" << ec.value() << " Message is " << ec.message();
    return;
  }
  // 取出队列首部元素并发送
  auto &send_data = _send_queue.front();
  send_data->_cur_len += bytes_transfered;
  if (send_data->_cur_len < send_data->_total_len)
  {
    this->_socket->async_write_some(asio::buffer(send_data->_msg + send_data->_cur_len,
                                                 send_data->_total_len - send_data->_cur_len),
                                    std::bind(&Session::WriteCallBackWithQueue, this, std::placeholders::_1, std::placeholders::_2));

    return;
  }
  _send_queue.pop();
  if (_send_queue.size() == 0)
    _sendpending = false;
  else
  {
    auto &send_data = _send_queue.front();
    this->_socket->async_write_some(asio::buffer(send_data->_msg + send_data->_cur_len,
                                                 send_data->_total_len - send_data->_cur_len),
                                    std::bind(&Session::WriteCallBackWithQueue, this, std::placeholders::_1, std::placeholders::_2));
  }
}

void Session::WriteToSocketWithQueue(const std::string &buf)
{
  _send_queue.emplace(new MsgNode(buf.c_str(), buf.length()));
  if (_sendpending)
  {
    return;
  }
  this->_socket->async_write_some(asio::buffer(buf),
                                  std::bind(&Session::WriteCallBackWithQueue, this, std::placeholders::_1, std::placeholders::_2));
  _sendpending = true;
}

// 不能与async_write_some混合使用
void Session::WriteAllToSocket(const std::string &buf)
{
  // 插入发送队列
  _send_queue.emplace(new MsgNode(buf.c_str(), buf.length()));
  // pending状态说明上一次有未发送完的数据
  if (_sendpending)
  {
    return;
  }
  // 异步发送数据，因为异步所以不会一下发送完
  this->_socket->async_send(asio::buffer(buf),
                            std::bind(&Session::WriteAllCallBack, this,
                                      std::placeholders::_1, std::placeholders::_2));
  _sendpending = true;
}

void Session::WriteAllCallBack(const boost::system::error_code &ec, std::size_t bytes_transferred)
{
  if (ec.value() != 0)
  {
    std::cout << "Error occured! Error code = "
              << ec.value()
              << ". Message: " << ec.message();
    return;
  }
  // 如果发送完，则pop出队首元素
  _send_queue.pop();
  // 如果队列为空，则说明所有数据都发送完,将pending设置为false
  if (_send_queue.empty())
  {
    _sendpending = false;
  }
  // 如果队列不是空，则继续将队首元素发送
  if (!_send_queue.empty())
  {
    auto &send_data = _send_queue.front();
    this->_socket->async_send(asio::buffer(send_data->_msg + send_data->_cur_len, send_data->_total_len - send_data->_cur_len),
                              std::bind(&Session::WriteAllCallBack,
                                        this, std::placeholders::_1, std::placeholders::_2));
  }
}

void Session::ReadCallBack(const boost::system::error_code &ec, std::size_t bytes_transfered)
{
  if (ec.value() != 0)
  {
    std::cout << "Error occured! Error code = "
              << ec.value()
              << ". Message: " << ec.message();
    return;
  }
  _recvnode -> _cur_len += bytes_transfered;
  if (_recvnode->_cur_len < _recvnode->_total_len){
    _socket->async_read_some(asio::buffer(_recvnode->_msg + _recvnode->_cur_len, _recvnode->_total_len - _recvnode->_cur_len),
    std::bind(&Session::ReadCallBack, this, std::placeholders::_1, std::placeholders::_2));
    return;
  }

  _recvpending = false;
  _recvnode = nullptr;
}

void Session::ReadFromSocket()
{
  if (_recvpending)
    return;
  _recvnode = std::make_shared<MsgNode>(RECVSIZE);
  _socket->async_read_some(asio::buffer(_recvnode->_msg, _recvnode->_total_len),
                           std::bind(&Session::ReadCallBack, this, std::placeholders::_1, std::placeholders::_2));

  _recvpending = true;
}


void Session::ReadAllFromSocket() {

  if (_recvpending) {
      return;
  }
  //可以调用构造函数直接构造，但不可用已经构造好的智能指针赋值
  /*auto _recv_nodez = std::make_unique<MsgNode>(RECVSIZE);
  _recv_node = _recv_nodez;*/
  _recvnode = std::make_shared<MsgNode>(RECVSIZE);
  _socket->async_receive(asio::buffer(_recvnode->_msg, _recvnode->_total_len), std::bind(&Session::ReadAllCallBack, 
      this,
      std::placeholders::_1, std::placeholders::_2));
      _recvpending = true;
}
void Session::ReadAllCallBack(const boost::system::error_code& ec, std::size_t bytes_transferred) {
  _recvnode->_cur_len += bytes_transferred;
  //将数据投递到队列里交给逻辑线程处理，此处略去
  //如果读完了则将标记置为false
  _recvpending = false;
  //指针置空
  _recvnode = nullptr;
}