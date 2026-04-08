#include "IOservicePool.h"

AsioIOServicePool::~AsioIOServicePool()
{
  std::cout << "~AsioIOServicePool destruct" << std::endl;
}

boost::asio::io_context &AsioIOServicePool::GetIOService()
{
    static std::mutex nextIOServiceMutex;
    std::lock_guard<std::mutex> lock(nextIOServiceMutex);
    auto &service = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size())
    {
        _nextIOService = 0;
    }
    return service;
}

AsioIOServicePool::AsioIOServicePool(std::size_t size):_ioServices(size == 0 ? 1 : size),
_works(size == 0 ? 1 : size), _nextIOService(0){
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));
    }
    //遍历多个ioservice，创建多个线程，每个线程内部启动ioservice
    for (std::size_t i = 0; i < _ioServices.size(); ++i) {
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}


void AsioIOServicePool::Stop(){
    for (auto& work : _works) {
        work.reset();
    }

    for (auto& io_service : _ioServices) {
        io_service.stop();
    }

    for (auto& t : _threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}