#include "Server.h"
#include "IOservicePool.h"
#include <iostream>

int main()
{
    try {
        auto pool = AsioIOServicePool::GetInstance();
        boost::asio::io_context  io_context;
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context,pool](auto, auto) {
            io_context.stop();
            pool->Stop();
            });
        Server s(io_context, 8080);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << endl;
    }
}