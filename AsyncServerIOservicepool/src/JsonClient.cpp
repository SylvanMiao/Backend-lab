#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <cstdint>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
using namespace std;
using namespace boost::asio::ip;
const int MAX_LENGTH = 1024 * 2;
const int HEAD_LENGTH = 2;
const int HEAD_TOTAL = 4;

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

		std::cout << "connected to server, input message and press enter. type 'quit' to exit." << std::endl;
		const std::uint16_t msgid = 1001;
		while (true) {
			std::cout << "> ";
			std::string user_input;
			if (!std::getline(std::cin, user_input)) {
				break;
			}
			if (user_input == "quit") {
				break;
			}
			if (user_input.empty()) {
				continue;
			}

			Json::Value req_root;
			req_root["id"] = msgid;
			req_root["data"] = user_input;
			std::string request = req_root.toStyledString();
			size_t request_length = request.length();
			if (request_length > static_cast<size_t>(MAX_LENGTH - HEAD_TOTAL)) {
				std::cout << "input too long, max payload is " << (MAX_LENGTH - HEAD_TOTAL) << " bytes" << std::endl;
				continue;
			}


      // 发送
			char send_data[MAX_LENGTH] = { 0 };
			std::uint16_t msgid_host = boost::asio::detail::socket_ops::host_to_network_short(msgid);
			memcpy(send_data, &msgid_host, HEAD_LENGTH);
			std::uint16_t request_host_length = boost::asio::detail::socket_ops::host_to_network_short(
				static_cast<std::uint16_t>(request_length));
			memcpy(send_data + HEAD_LENGTH, &request_host_length, HEAD_LENGTH);
			memcpy(send_data + HEAD_TOTAL, request.c_str(), request_length);
			boost::asio::write(sock, boost::asio::buffer(send_data, request_length + HEAD_TOTAL));


      // 读头部
			char reply_head[HEAD_TOTAL] = { 0 };
			boost::asio::read(sock, boost::asio::buffer(reply_head, HEAD_TOTAL));

			std::uint16_t resp_msgid = 0;
			memcpy(&resp_msgid, reply_head, HEAD_LENGTH);
			std::uint16_t msglen = 0;
			memcpy(&msglen, reply_head + HEAD_LENGTH, HEAD_LENGTH);
			resp_msgid = boost::asio::detail::socket_ops::network_to_host_short(resp_msgid);
			msglen = boost::asio::detail::socket_ops::network_to_host_short(msglen);
			if (msglen == 0 || msglen > MAX_LENGTH) {
				std::cout << "invalid msg length: " << msglen << std::endl;
				break;
			}

      // 读消息体
			char msg[MAX_LENGTH] = { 0 };
			size_t msg_length = boost::asio::read(sock, boost::asio::buffer(msg, msglen));
			Json::Value resp_root;
			Json::Reader reader;
			if (!reader.parse(std::string(msg, msg_length), resp_root)) {
				std::cout << "parse response json failed" << std::endl;
				continue;
			}
			std::cout << "msg id is " << resp_msgid << " msg is " << resp_root["data"] << std::endl;
		}
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << endl;
	}
	return 0;
}


