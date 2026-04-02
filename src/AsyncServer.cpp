#include <iostream>
#include "AsyncServer.h"

// =================================== session func below ======================================

void Session::Send(char *msg, int max_length)
{
  bool pending = false;
  std::lock_guard<std::mutex> lock(_send_lock);
  if (_send_que.size() > 0)
  {
    pending = true;
  }
  if (_send_que.size() > MAX_SENDQUEUE_SIZE){
    std::cout << "session: " << _uuid << "send que fulled, size is: " << MAX_SENDQUEUE_SIZE << std::endl;
    return;
  }
  _send_que.push(std::make_shared<MsgNode>(msg, max_length));
  if (pending)
  {
    return;
  }
  auto &msgnode = _send_que.front();
  boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
                           std::bind(&Session::handle_write, this, std::placeholders::_1, shared_from_this()));
}

std::string &Session::GetUuid()
{
  return _uuid;
}

void Session::Start()
{
  _recv_head_node->Clear();
  boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_SIZE),
                          std::bind(&Session::handle_read_head, this, std::placeholders::_1, std::placeholders::_2, shared_from_this()));
}

void Session::Close()
{
  // 清理发送队列并安全关闭 socket，然后通知 Server 清除会话
  {
    std::lock_guard<std::mutex> lock(_send_lock);
    while (!_send_que.empty())
      _send_que.pop();
  }

  boost::system::error_code ec;
  if (_socket.is_open())
  {
    // 尝试有序关闭连接，忽略可能的错误
    _socket.shutdown(tcp::socket::shutdown_both, ec);
    _socket.close(ec);
  }
}

void Session::handle_read_head(const boost::system::error_code &error, size_t bytes_transfered, std::shared_ptr<Session> _self_shared)
{
  if (!error)
  {
    if (bytes_transfered != HEAD_SIZE)
    {
      std::cout << "invalid header size is " << bytes_transfered << std::endl;
      _server->ClearSession(_uuid);
      return;
    }

    std::uint16_t data_len_net = 0;
    memcpy(&data_len_net, _recv_head_node->_data, HEAD_SIZE);
    int data_len = boost::asio::detail::socket_ops::network_to_host_short(data_len_net);
    std::cout << "data_len is " << data_len << std::endl;

    if (data_len <= 0 || data_len > max_length)
    {
      std::cout << "invalid data length is " << data_len << std::endl;
      _server->ClearSession(_uuid);
      return;
    }

    _recv_msg_node = std::make_shared<MsgNode>(data_len);
    boost::asio::async_read(_socket, boost::asio::buffer(_recv_msg_node->_data, data_len),
                            std::bind(&Session::handle_read_body, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
  }
  else
  {
    if (error != boost::asio::error::eof)
    {
      std::cout << "handle read failed, error is " << error.what() << std::endl;
    }
    // Close();
    _server->ClearSession(_uuid);
  }
}

void Session::handle_read_body(const boost::system::error_code &error, size_t bytes_transfered, std::shared_ptr<Session> _self_shared)
{
  if (!error)
  {
    if (bytes_transfered != static_cast<size_t>(_recv_msg_node->_total_len))
    {
      std::cout << "invalid body size is " << bytes_transfered << std::endl;
      _server->ClearSession(_uuid);
      return;
    }

    _recv_msg_node->_cur_len = static_cast<int>(bytes_transfered);
    _recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';
    std::cout << "receive data is " << _recv_msg_node->_data << std::endl;

    Send(_recv_msg_node->_data, _recv_msg_node->_total_len);

    _recv_head_node->Clear();
    boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_SIZE),
                            std::bind(&Session::handle_read_head, this, std::placeholders::_1, std::placeholders::_2, _self_shared));
  }
  else
  {
    if (error != boost::asio::error::eof)
    {
      std::cout << "handle read body failed, error is " << error.what() << std::endl;
    }
    _server->ClearSession(_uuid);
  }
}

void Session::handle_write(const boost::system::error_code &error, std::shared_ptr<Session> _self_shared)
{
  if (!error)
  {
    std::lock_guard<std::mutex> lock(_send_lock);
    _send_que.pop();
    if (!_send_que.empty())
    {
      auto &msgnode = _send_que.front();
      boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
                               std::bind(&Session::handle_write, this, std::placeholders::_1, _self_shared));
    }
  }
  else
  {
    std::cout << "handle write failed, error is " << error.what() << std::endl;
    _server->ClearSession(_uuid);
  }
}
// =================================== session func above ======================================

// =================================== server func below ======================================

Server::Server(boost::asio::io_context &ioc, short port) : _ioc(ioc),
                                                           _acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
  std::cout << "Server Start on port :" << port << std::endl;
  start_accept();
}

void Server::ClearSession(std::string _uuid)
{
  _sessions.erase(_uuid);
}

void Server::start_accept()
{
  std::shared_ptr<Session> new_session = std::make_shared<Session>(_ioc, this);
  _acceptor.async_accept(new_session->Socket(),
                         std::bind(&Server::handle_accept, this, new_session, std::placeholders::_1));
}

void Server::handle_accept(std::shared_ptr<Session> new_session, const boost::system::error_code &error)
{
  if (!error)
  {
    new_session->Start();
    _sessions.insert(std::make_pair(new_session->GetUuid(), new_session));
  }
  else
  {
    // delete new_session;
  }

  start_accept();
}

// =================================== server func above ======================================

int main()
{
  try
  {
    boost::asio::io_context ioc;
    Server s(ioc, 8080);
    ioc.run();
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
  }
}