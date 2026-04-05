#include "Server.h"
#include <iostream>

Server::Server(boost::asio::io_context& io_context, short port):_io_context(io_context), _port(port),
_acceptor(io_context, tcp::endpoint(tcp::v4(),port))
{
	cout << "Server start success, listen on port : " << _port << endl;
	StartAccept();
}

void Server::HandleAccept(shared_ptr<Session> new_session, const boost::system::error_code& error){
	if (!error) {
		new_session->Start();
		_sessions.insert(make_pair(new_session->GetUuid(), new_session));
	}
	else {
		cout << "session accept failed, error is " << error.what() << endl;
	}

	StartAccept();
}

void Server::StartAccept() {
	shared_ptr<Session> new_session = make_shared<Session>(_io_context, this);
	_acceptor.async_accept(new_session->GetSocket(), std::bind(&Server::HandleAccept, this, new_session, placeholders::_1));
}

void Server::ClearSession(std::string uuid) {
	_sessions.erase(uuid);
}


int main()
{
	try {
		boost::asio::io_context  io_context;
		boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
		signals.async_wait([&io_context](auto, auto) {
			io_context.stop();
			});
		Server s(io_context, 8080);
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << endl;
	}
}