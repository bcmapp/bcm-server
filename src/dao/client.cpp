#include <brpc/channel.h>

#include "dao/client.h"
#include "rpc_impl/contacts_rpc_impl.h"
#include "rpc_impl/onetime_keys_rpc_impl.h"
#include "rpc_impl/signup_challenges_rpc_impl.h"
#include "rpc_impl/accounts_rpc_impl.h"
#include "rpc_impl/stored_messages_rpc_impl.h"
#include "rpc_impl/group_user_rpc_impl.h"
#include "rpc_impl/group_rpc_impl.h"
#include "rpc_impl/group_msg_rpc_impl.h"
#include "rpc_impl/limiters_rpc_impl.h"
#include "rpc_impl/sys_messages_rpc_impl.h"
#include "rpc_impl/utilities_rpc_impl.h"
#include "rpc_impl/pending_group_user_rpc_impl.h"
#include "rpc_impl/limiter_configurations_rpc_impl.h"
#include "rpc_impl/group_keys_rpc_impl.h"
#include "rpc_impl/qr_code_group_users_rpc_impl.h"
#include "rpc_impl/opaque_data_rpc_impl.h"
#include "dao_impl_creator.h"
#include "utils/log.h"


namespace bcm {
namespace dao {

// A Channel represents a communication line to a Server. Notice that
// Channel is thread-safe and can be shared by all threads in your program.
brpc::Channel channel;

bool initialize(const bcm::DaoConfig& config)
{
    if (config.clientImpl == REMOTE) {
        // Initialize the channel, NULL means using default options.
        brpc::ChannelOptions options;
        options.protocol = config.remote.proto;
        options.connection_type = config.remote.connType;
        options.timeout_ms = config.timeout;
        options.max_retry = config.remote.retries;
        options.mutable_ssl_options()->client_cert.certificate = config.remote.certPath.c_str();
        options.mutable_ssl_options()->client_cert.private_key = config.remote.keyPath.c_str();

        if (channel.Init(config.remote.hosts.c_str(), config.remote.balancer.c_str(), &options) != 0) {
            LOGE << "Fail to initialize channel";
            return false;
        }

        DaoImplCreator<Accounts>::registerFunc([] () {
            return std::make_shared<AccountsRpcImp>(&channel);
        });

        DaoImplCreator<Contacts>::registerFunc([] () {
            return std::make_shared<ContactsRpcImp>(&channel);
        });

        DaoImplCreator<SignUpChallenges>::registerFunc([] () {
            return std::make_shared<SignupChallengesRpcImpl>(&channel);
        });

        DaoImplCreator<StoredMessages>::registerFunc([] () {
            return std::make_shared<StoredMessagesRpcImp>(&channel);
        });

        DaoImplCreator<OnetimeKeys>::registerFunc([] () {
            return std::make_shared<OnetimeKeysRpcImpl>(&channel);
        });

        DaoImplCreator<GroupUsers>::registerFunc([] () {
            return std::make_shared<GroupUsersRpcImpl>(&channel);
        });

        DaoImplCreator<Groups>::registerFunc([] () {
            return std::make_shared<GroupsRpcImp>(&channel);
        });
        DaoImplCreator<GroupMsgs>::registerFunc([] () {
            return std::make_shared<GroupMsgsRpcImp>(&channel);
        });
        DaoImplCreator<Limiters>::registerFunc([] () {
            return std::make_shared<LimitersRpcImpl>(&channel);
        });

        DaoImplCreator<SysMsgs>::registerFunc([] () {
            return std::make_shared<SysMsgsRpcImpl>(&channel);
        });

        DaoImplCreator<MasterLease>::registerFunc([] () {
            return std::make_shared<MasterLeaseRpcImpl>(&channel);
        });

        DaoImplCreator<PendingGroupUsers>::registerFunc([] () {
            return std::make_shared<PendingGroupUserRpcImpl>(&channel);
        });

        DaoImplCreator<LimiterConfigurations>::registerFunc([] () {
            return std::make_shared<LimiterConfigurationRpcImpl>(&channel);
        });


        DaoImplCreator<GroupKeys>::registerFunc([] () {
            return std::make_shared<GroupKeysRpcImpl>(&channel);
        });

        DaoImplCreator<QrCodeGroupUsers>::registerFunc([] () {
            return std::make_shared<QrCodeGroupUsersRpcImpl>(&channel);
        });

        DaoImplCreator<OpaqueData>::registerFunc([] () {
            return std::make_shared<OpaqueDataRpcImpl>(&channel);
        });

    } else {
        LOGE << "failed to initialize db interface";
        return false;
    }

    return true;
}

std::shared_ptr<Accounts> ClientFactory::accounts()
{
    return DaoImplCreator<Accounts>::create();
}
std::shared_ptr<Contacts> ClientFactory::contacts()
{
    return DaoImplCreator<Contacts>::create();
}
std::shared_ptr<SignUpChallenges> ClientFactory::signupChallenges()
{
    return DaoImplCreator<SignUpChallenges>::create();
}
std::shared_ptr<StoredMessages> ClientFactory::storedMessages()
{
    return DaoImplCreator<StoredMessages>::create();
}
std::shared_ptr<OnetimeKeys> ClientFactory::onetimeKeys()
{
    return DaoImplCreator<OnetimeKeys>::create();
}

std::shared_ptr<Groups> ClientFactory::groups()
{
    return DaoImplCreator<Groups>::create();
}
std::shared_ptr<GroupUsers> ClientFactory::groupUsers()
{
    return DaoImplCreator<GroupUsers>::create();
}
std::shared_ptr<GroupMsgs> ClientFactory::groupMsgs()
{
    return DaoImplCreator<GroupMsgs>::create();
}

std::shared_ptr<PendingGroupUsers> ClientFactory::pendingGroupUsers()
{
    return DaoImplCreator<PendingGroupUsers>::create();
}

std::shared_ptr<Limiters> ClientFactory::limiters()
{
    return DaoImplCreator<Limiters>::create();
}

std::shared_ptr<SysMsgs> ClientFactory::sysMsgs()
{
    return DaoImplCreator<SysMsgs>::create();
}

std::shared_ptr<MasterLease> ClientFactory::masterLease()
{
    return DaoImplCreator<MasterLease>::create();
}

std::shared_ptr<LimiterConfigurations> ClientFactory::limiterConfigurations()
{
    return DaoImplCreator<LimiterConfigurations>::create();
}

std::shared_ptr<GroupKeys> ClientFactory::groupKeys()
{
    return DaoImplCreator<GroupKeys>::create();
}

std::shared_ptr<QrCodeGroupUsers> ClientFactory::qrCodeGroupUsers()
{
    return DaoImplCreator<QrCodeGroupUsers>::create();
}

std::shared_ptr<OpaqueData> ClientFactory::opaqueData()
{
    return DaoImplCreator<OpaqueData>::create();
}

}  // namespace dao
}  // namespace bcm
