#include "Session.h"
#include "Server.h"
#include <iostream>
#include <sstream>
#include <cstdint>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

Session::Session(boost::asio::io_context& io_context, Server* server):
	_socket(io_context), _server(server), _b_close(false){
	boost::uuids::uuid  a_uuid = boost::uuids::random_generator()();
	_uuid = boost::uuids::to_string(a_uuid);
	_recv_head_node = make_shared<MsgNode>(HEAD_TOTAL_LEN);
}
Session::~Session() {
	std::cout << "~Session destruct" << endl;
}

tcp::socket& Session::GetSocket() {
	return _socket;
}

std::string& Session::GetUuid() {
	return _uuid;
}

void Session::Start(){
	_recv_head_node->Clear();
	boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
		std::bind(&Session::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2, SharedSelf()));
}

void Session::Send(std::string msg, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _uuid << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg.c_str(), msg.length(), msgid));
	if (send_que_size > 0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
		std::bind(&Session::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void Session::Send(char* msg, short max_length, short msgid) {
	std::lock_guard<std::mutex> lock(_send_lock);
	int send_que_size = _send_que.size();
	if (send_que_size > MAX_SENDQUE) {
		std::cout << "session: " << _uuid << " send que fulled, size is " << MAX_SENDQUE << endl;
		return;
	}

	_send_que.push(make_shared<SendNode>(msg, max_length, msgid));
	if (send_que_size>0) {
		return;
	}
	auto& msgnode = _send_que.front();
	boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len), 
		std::bind(&Session::HandleWrite, this, std::placeholders::_1, SharedSelf()));
}

void Session::Close() {
	_socket.close();
	_b_close = true;
}

std::shared_ptr<Session>Session::SharedSelf() {
	return shared_from_this();
}

void Session::HandleWrite(const boost::system::error_code& error, std::shared_ptr<Session> shared_self) {
	//增加异常处理
	try {
		if (!error) {
			std::lock_guard<std::mutex> lock(_send_lock);
			//cout << "send data " << _send_que.front()->_data+HEAD_LENGTH << endl;
			_send_que.pop();
			if (!_send_que.empty()) {
				auto& msgnode = _send_que.front();
				boost::asio::async_write(_socket, boost::asio::buffer(msgnode->_data, msgnode->_total_len),
					std::bind(&Session::HandleWrite, this, std::placeholders::_1, shared_self));
			}
		}
		else {
			std::cout << "handle write failed, error is " << error.what() << endl;
			Close();
			_server->ClearSession(_uuid);
		}
	}
	catch (std::exception& e) {
		std::cerr << "Exception code : " << e.what() << endl;
	}
	
}

void Session::HandleReadHead(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<Session> shared_self) {
	try {
		if (error) {
			std::cout << "handle read head failed, error is " << error.what() << endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

		if (bytes_transferred != HEAD_TOTAL_LEN) {
			std::cout << "invalid head length is " << bytes_transferred << endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

    // 获取消息id
		std::uint16_t msg_id_net = 0;
		memcpy(&msg_id_net, _recv_head_node->_data, HEAD_ID_LEN);
		short msg_id = static_cast<short>(boost::asio::detail::socket_ops::network_to_host_short(msg_id_net));
		if (msg_id <= 0 || msg_id > MAX_LENGTH) {
			std::cout << "invalid msg_id is " << msg_id << std::endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}
		std::cout << "msg_id is " << msg_id << endl;

    // 获取消息长度
		std::uint16_t msg_len_net = 0;
		memcpy(&msg_len_net, _recv_head_node->_data + HEAD_ID_LEN, HEAD_DATA_LEN);
		short msg_len = static_cast<short>(boost::asio::detail::socket_ops::network_to_host_short(msg_len_net));
		std::cout << "msg_len is " << msg_len << endl;

		if (msg_len <= 0 || msg_len > MAX_LENGTH) {
			std::cout << "invalid data length is " << msg_len << endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

    // 接收消息体
		_recv_msg_node = make_shared<RecvNode>(msg_len, msg_id);
		boost::asio::async_read(_socket, boost::asio::buffer(_recv_msg_node->_data, _recv_msg_node->_total_len),
			std::bind(&Session::HandleReadBody, this, std::placeholders::_1, std::placeholders::_2, shared_self));
	}
	catch (std::exception& e) {
		std::cout << "Exception code is " << e.what() << endl;
	}
}

void Session::HandleReadBody(const boost::system::error_code& error, size_t bytes_transferred, std::shared_ptr<Session> shared_self) {
	try {
		if (error) {
			std::cout << "handle read body failed, error is " << error.what() << endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

		if (bytes_transferred != static_cast<size_t>(_recv_msg_node->_total_len)) {
			std::cout << "invalid body length is " << bytes_transferred << endl;
			Close();
			_server->ClearSession(_uuid);
			return;
		}

		_recv_msg_node->_cur_len = _recv_msg_node->_total_len;
		_recv_msg_node->_data[_recv_msg_node->_total_len] = '\0';

		Json::Reader reader;
		Json::Value root;
		reader.parse(std::string(_recv_msg_node->_data, _recv_msg_node->_total_len), root);
		std::cout << "recevie msg id  is " << root["id"].asInt() << " msg data is "
			<< root["data"].asString() << endl;
		root["data"] = "server has received msg, msg data is " + root["data"].asString();
		std::string return_str = root.toStyledString();
		Send(return_str, root["id"].asInt());

		_recv_head_node->Clear();
		boost::asio::async_read(_socket, boost::asio::buffer(_recv_head_node->_data, HEAD_TOTAL_LEN),
			std::bind(&Session::HandleReadHead, this, std::placeholders::_1, std::placeholders::_2, shared_self));
	}
	catch (std::exception& e) {
		std::cout << "Exception code is " << e.what() << endl;
	}
}
