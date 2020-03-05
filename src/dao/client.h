#pragma once

#include <string>
#include <memory>
#include "accounts.h"
#include "contacts.h"
#include "sign_up_challenges.h"
#include "stored_messages.h"
#include "onetime_keys.h"

#include "groups.h"
#include "group_msgs.h"
#include "group_users.h"
#include "limiters.h"
#include "sys_msgs.h"
#include "utilities.h"
#include "pending_group_users.h"
#include "limiter_configurations.h"
#include "group_keys.h"
#include "qr_code_group_users.h"
#include "opaque_data.h"

#include "config/dao_config.h"


namespace bcm {
namespace dao {

const std::string LOCAL= "local";
const std::string REMOTE = "remote";

extern bool initialize(const bcm::DaoConfig& config);

class ClientFactory {
public:
    static std::shared_ptr<Accounts> accounts();
    static std::shared_ptr<Contacts> contacts();
    static std::shared_ptr<SignUpChallenges> signupChallenges();
    static std::shared_ptr<StoredMessages> storedMessages();
    static std::shared_ptr<OnetimeKeys> onetimeKeys();

    static std::shared_ptr<Groups> groups();
    static std::shared_ptr<GroupUsers> groupUsers();
    static std::shared_ptr<GroupMsgs> groupMsgs();
    static std::shared_ptr<PendingGroupUsers> pendingGroupUsers();
    static std::shared_ptr<Limiters> limiters();

    static std::shared_ptr<SysMsgs> sysMsgs();
    static std::shared_ptr<MasterLease> masterLease();

    static std::shared_ptr<LimiterConfigurations> limiterConfigurations();
    static std::shared_ptr<GroupKeys> groupKeys();
    static std::shared_ptr<QrCodeGroupUsers> qrCodeGroupUsers();
    static std::shared_ptr<OpaqueData> opaqueData();
};

}  // namespace dao
}  // namespace bcm
