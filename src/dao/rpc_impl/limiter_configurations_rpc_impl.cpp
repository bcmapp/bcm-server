
#include <boost/algorithm/string/split.hpp>
#include "limiter_configurations_rpc_impl.h"
#include "utils/log.h"

namespace bcm {
namespace dao {

static const std::string kHashKey("limiter_config");


static LimiterConfigurations::LimiterConfigs::value_type toLimitRule(const ::bcm::dao::rpc::KVPair& pair)
{
    std::vector<std::string> words;
    boost::split(words, pair.value(), boost::is_any_of(":"));
    LimitRule item(std::stoll(words[0]), std::stoll(words[1]));
    return LimiterConfigurations::LimiterConfigs::value_type(pair.key(), std::move(item));
}

static void fromLimitRule(const LimiterConfigurations::LimiterConfigs::value_type& entry, 
                                   ::bcm::dao::rpc::KVPair* ptr)
{
    if (ptr == nullptr) {
        return;
    }
    ptr->set_key(entry.first);
    std::ostringstream oss;
    oss << entry.second.period << ":" << entry.second.count;
    ptr->set_value(oss.str());
}

LimiterConfigurationRpcImpl::LimiterConfigurationRpcImpl(brpc::Channel* ch) : stub(ch)
{

}

ErrorCode LimiterConfigurationRpcImpl::load(LimiterConfigs& configs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::PageGetKVPairReq request;
    bcm::dao::rpc::PageGetKVPairResp response;
    brpc::Controller cntl;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    request.set_hashkey(kHashKey);
    request.set_startsortkey("");
    request.set_limit(0);

    try {
        stub.pageGetKVPairs(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());

        if (ErrorCode::ERRORCODE_SUCCESS == ec) {
            for (auto& item : response.data()) {
                configs.emplace(std::move(toLimitRule(item)));
            }
        }

        return ec;

    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode LimiterConfigurationRpcImpl::get(const std::set<std::string>& keys, LimiterConfigs& configs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::MultiGetKVPairReq request;
    bcm::dao::rpc::MultiGetKVPairResp response;
    brpc::Controller cntl;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    request.set_hashkey(kHashKey);
    for (const auto& k : keys) {
        request.add_keys(k);
    }

    try {
        stub.getKVPairs(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        ErrorCode ec = static_cast<ErrorCode>(response.rescode());

        if (ErrorCode::ERRORCODE_SUCCESS == ec) {
            for (auto& item : response.data()) {
                configs.emplace(std::move(toLimitRule(item)));
            }
        }

        return ec;

    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

ErrorCode LimiterConfigurationRpcImpl::set(const LimiterConfigs& configs)
{
    if (stub.channel() == nullptr) {
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }

    bcm::dao::rpc::SetKVPairReq request;
    bcm::dao::rpc::SetKVPairResp response;
    brpc::Controller cntl;
    cntl.set_log_id(logId.fetch_add(1, std::memory_order_relaxed));
    request.set_hashkey(kHashKey);
    for (const auto& kv : configs) {
        ::bcm::dao::rpc::KVPair* p = request.add_data();
        if (!p) {
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        fromLimitRule(kv, p);
    }

    try {
        stub.setKVPairs(&cntl, &request, &response, nullptr);
        if (cntl.Failed()) {
            LOGE << "ErrorCode: " << cntl.ErrorCode() << ", ErrorText: " << berror(cntl.ErrorCode());
            return ErrorCode::ERRORCODE_INTERNAL_ERROR;
        }
        return static_cast<ErrorCode>(response.rescode());
    } catch (const std::exception& ex) {
        LOGE << ex.what();
        return ErrorCode::ERRORCODE_INTERNAL_ERROR;
    }
}

std::atomic<uint64_t> LimiterConfigurationRpcImpl::logId(0);

}
}
