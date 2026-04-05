#pragma once
#include <boost/asio.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <queue>
#include <mutex>
#include <memory>
#include "const.h"
#include "MsgNode.h"
using namespace std;

using boost::asio::ip::tcp;
class Server;

class Session: public std::enable_shared_from_this<Session>
{
public:
	Session(boost::asio::io_context& io_context, Server* server);
	~Session();
	tcp::socket& GetSocket();
	std::string& GetUuid();
	void Start();
	void Send(char* msg,  short max_length, short msgid);
	void Send(std::string msg, short msgid);
	void Close();
	std::shared_ptr<Session> SharedSelf();
private:
	void HandleReadHead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<Session> shared_self);
	void HandleReadBody(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<Session> shared_self);
	void HandleWrite(const boost::system::error_code& error, std::shared_ptr<Session> shared_self);
	tcp::socket _socket;
	std::string _uuid;
	char _data[MAX_LENGTH];
	Server* _server;
	bool _b_close;
	std::queue<shared_ptr<MsgNode> > _send_que;
	std::mutex _send_lock;
	//收到的消息结构
	std::shared_ptr<MsgNode> _recv_msg_node;
	//收到的头部结构
	std::shared_ptr<MsgNode> _recv_head_node;
};

