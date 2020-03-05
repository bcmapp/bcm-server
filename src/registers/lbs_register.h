#pragma once

#include <fiber/fiber_timer.h>
#include <config/lbs_config.h>
#include <config/service_config.h>

namespace bcm {

using tcp = boost::asio::ip::tcp;

class LbsRegister : public FiberTimer::Task {
public:
    static constexpr int64_t kKeepAliveInterval = 3000;

    LbsRegister(LbsConfig lbsConfig, ServiceConfig serviceConfig);
    ~LbsRegister();

private:
    void run() override;
    void cancel() override;
    int64_t lastExecTimeInMilli() override;

private:
    enum InvokeType {
        REGISTER,
        KEEP_ALIVE,
        UNREGISTER,
    };
    void invokeLbsApi(InvokeType type);

private:
    std::vector<std::string> m_services;
    LbsConfig m_config;
    bool m_isRegistered{false};
    int64_t m_execTime{0};
};

}