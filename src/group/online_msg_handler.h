#pragma once

#include <string>
#include <set>
#include <thread>

#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "online_msg_member_mgr.h"
#include "dispatcher/dispatch_manager.h"
#include "config/noise_config.h"

namespace bcm {

class OnlineMsgHandler {
public:
    OnlineMsgHandler(std::shared_ptr<DispatchManager> dispatchMgr,
                     OnlineMsgMemberMgr& memberMgr,
                     const NoiseConfig& cfg);
    ~OnlineMsgHandler();

    void handleMessage(const std::string& chan, const nlohmann::json& msgObj);

private:
    void handleGroupMessage(const std::string& chan,
                            const nlohmann::json& msgObj,
                            OnlineMsgMemberMgr::UserSet& targetUids,
                            std::string& message);

    void generateNoiseForGroupMessage(const std::string& chan,
                                      const nlohmann::json& msgObj,
                                      const OnlineMsgMemberMgr::UserSet& onlineUids,
                                      OnlineMsgMemberMgr::UserSet& targetUids,
                                      std::string& message);

    void pickNoiseReceivers(uint64_t gid,
                            const OnlineMsgMemberMgr::UserSet& onlineUids,
                            OnlineMsgMemberMgr::UserSet& targetUids);
private:
    std::shared_ptr<DispatchManager> m_dispatchMgr;
    OnlineMsgMemberMgr& m_memberMgr;
    NoiseConfig m_noiseCfg;
    std::string m_lastNoiseUid;
};

} // namespace bcm
