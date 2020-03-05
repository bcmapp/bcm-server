#pragma once

#include <thread>
#include <boost/asio.hpp>

namespace bcm {

class IoCtxPool {
public:
    typedef std::shared_ptr<boost::asio::io_context> io_context_ptr;
    typedef std::shared_ptr<boost::asio::io_context::work> work_ptr;

    IoCtxPool(int size);
    ~IoCtxPool();

    io_context_ptr getIoCtxByGid(uint64_t gid);
    io_context_ptr getIoCtxByUid(const std::string& uid);
    void shutdown(bool force = false);

private:
    std::vector<std::thread> m_threads;
    std::vector<io_context_ptr> m_iocs;
    std::vector<work_ptr> m_works;
    std::atomic<bool> m_stopped;
};

} // namespace bcm