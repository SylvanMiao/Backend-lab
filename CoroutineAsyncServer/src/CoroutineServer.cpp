#include <iostream>
#include "MsgNode.h"
#include <thread>
#include <csignal>
#include <mutex>

#include "Server.h"
#include "IOservicePool.h"

int main()
{
  try
  {
    auto pool = AsioIOServicePool::GetInstance();
    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context, &pool](auto, auto)
    {
      io_context.stop();
      pool->Stop(); });
    Server s(io_context, 8080);
    io_context.run();
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << '\n';
  }
}