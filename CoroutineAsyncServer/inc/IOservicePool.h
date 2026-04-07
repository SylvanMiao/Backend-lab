#pragma once
#include <boost/asio.hpp>
#include <vector>
#include <mutex>
#include "Singleton.h"

class AsioIOServicePool:public Singleton<AsioIOServicePool>
{
    friend Singleton<AsioIOServicePool>;
public:
    // 服务io上下文
    using IOService = boost::asio::io_context;
    // 防止iocontext退出的绑定量
    using Work = boost::asio::io_context::work;
    using WorkPtr = std::unique_ptr<Work>;
    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;
    // 使用 round-robin 的方式返回一个 io_service
    boost::asio::io_context& GetIOService();
    void Stop();
private:
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    std::vector<IOService> _ioServices;
    std::vector<WorkPtr> _works;
    std::vector<std::thread> _threads;
    std::size_t _nextIOService;
};