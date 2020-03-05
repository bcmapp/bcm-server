#pragma once

#include <thread>
#include <boost/asio.hpp>

namespace bcm {

class IoCtxExecutor {
public:
    typedef std::shared_ptr<boost::asio::io_context> io_context_ptr;
    typedef std::shared_ptr<boost::asio::io_context::work> work_ptr;

    IoCtxExecutor(int size);
    ~IoCtxExecutor();

    template <class Fn>
    void execInPool(Fn&& fn)
    {
        m_ioc->post(std::forward<Fn>(fn));
    }

    io_context_ptr io_context()
    {
        return m_ioc;
    }
    
    void stop(bool force = false);

private:
    io_context_ptr m_ioc;
    work_ptr m_work;
    std::vector<std::thread> m_threads;
};

} // namespace bcm