#pragma once

#include <boost/asio.hpp>
#include <boost/fiber/all.hpp>

namespace bcm {

namespace asio = boost::asio;
namespace fibers = boost::fibers;

class FiberPool {
public:
    explicit FiberPool(size_t concurrency);
    ~FiberPool() = default;

    void run(const std::string& name = "fiber.worker");
    void stop();
    asio::io_context& getIOContext();

    template <typename Func, typename ... Args>
    static void post(asio::io_context& ioc, Func&& func, Args&&... args)
    {
        asio::post(ioc, [=]() {
            fibers::fiber(func, (args)...).detach();
        });
    }
    static asio::io_context* getThreadIOContext();

private:
    std::atomic_size_t m_nextIocIndex{0};
    std::vector<std::thread> m_threads;
    std::vector<std::shared_ptr<asio::io_context>> m_iocs;
};

}