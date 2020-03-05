#include <cinttypes>
#include "../test_common.h"
#include "dao/client.h"
#include "../../src/proto/dao/stored_message.pb.h"
#include "../../src/proto/dao/group_msg.pb.h"
#include "../../src/proto/dao/group_user.pb.h"
#include "../../src/proto/dao/sys_msg.pb.h"
#include "../../src/proto/dao/account.pb.h"
#include "../../src/proto/brpc/rpc_utilities.pb.h"
#include "dao/rpc_impl/group_keys_rpc_impl.h"
#include <thread>

void initialize()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    bcm::DaoConfig config;
    config.remote.hosts = "localhost:34567";
    config.remote.proto = "baidu_std";
    config.remote.connType = "";
    config.timeout = 60000;
    config.remote.retries = 3;
    config.remote.balancer = "";
    config.remote.keyPath = "./key.pem";
    config.remote.certPath = "./cert.pem";
    config.clientImpl = bcm::dao::REMOTE;

    REQUIRE(bcm::dao::initialize(config) == true);

    initialized = true;
}

bool groupUsersEquals(const ::bcm::GroupUser& l, const ::bcm::GroupUser& r)
{
    REQUIRE(l.gid() == r.gid());
    REQUIRE(l.uid() == r.uid());
    REQUIRE(l.nick() == r.nick());
    REQUIRE(l.updatetime() == r.updatetime());
    REQUIRE(l.lastackmid() == r.lastackmid());
    REQUIRE(l.role() == r.role());
    REQUIRE(l.status() == r.status());
    REQUIRE(l.encryptedkey() == r.encryptedkey());
    REQUIRE(l.createtime() == r.createtime());
    REQUIRE(l.nickname() == r.nickname());
    REQUIRE(l.profilekeys() == r.profilekeys());
    REQUIRE(l.groupnickname() == r.groupnickname());
    REQUIRE(l.groupinfosecret() == r.groupinfosecret());
    REQUIRE(l.proof() == r.proof());
    return true;
}

bool groupEquals(const ::bcm::Group& l, const ::bcm::Group& r)
{
    REQUIRE(l.gid() == r.gid());
    REQUIRE(l.lastmid() == r.lastmid());
    REQUIRE(l.name() == r.name());
    REQUIRE(l.icon() == r.icon());
    REQUIRE(l.permission() == r.permission());
    REQUIRE(l.updatetime() == r.updatetime());
    REQUIRE(l.broadcast() == r.broadcast());
    REQUIRE(l.status() == r.status());
    REQUIRE(l.channel() == r.channel());
    REQUIRE(l.intro() == r.intro());
    REQUIRE(l.createtime() == r.createtime());
    REQUIRE(l.key() == r.key());
    REQUIRE(l.encryptstatus() == r.encryptstatus());
    REQUIRE(l.plainchannelkey() == r.plainchannelkey());
    REQUIRE(l.notice() == r.notice());
    REQUIRE(l.shareqrcodesetting() == r.shareqrcodesetting());
    REQUIRE(l.ownerconfirm() == r.ownerconfirm());
    REQUIRE(l.sharesignature() == r.sharesignature());
    REQUIRE(l.shareandownerconfirmsignature() == r.shareandownerconfirmsignature());
    REQUIRE(l.version() == r.version());
    REQUIRE(l.encryptedgroupinfosecret() == r.encryptedgroupinfosecret());
    return true;

}

bool pendingGroupUserEquals(const bcm::PendingGroupUser& l, const bcm::PendingGroupUser& r)
{
    REQUIRE(l.gid() == r.gid());
    REQUIRE(l.uid() == r.uid());
    REQUIRE(l.inviter() == r.inviter());
    REQUIRE(l.signature() == r.signature());
    REQUIRE(l.comment() == r.comment());
    return true;
}

TEST_CASE("contact")
{
    initialize();

    std::shared_ptr<bcm::dao::Contacts> pContact = bcm::dao::ClientFactory::contacts();
    REQUIRE(pContact != nullptr);

    std::string contact = "hello_contact";

    std::map<std::string, std::string>  inKey;
    inKey["1"] = "1111";
    inKey["2"] = "2222";

    std::string testUid = "uid";
    std::string testUidMissed = "uid2";
    std::string testDeviceId = "1";

    REQUIRE(pContact->setInParts(testUid, inKey) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    std::vector<std::string> vecPart;
    vecPart.push_back("1");
    vecPart.push_back("2");

    std::map<std::string, std::string> contacts;
    REQUIRE(pContact->getInParts(testUid, vecPart, contacts) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(contacts == inKey);

    std::cout << "getInParts: ";
    for (const auto& it : contacts) {
        std::cout << "(" << it.first << "," << it.second << "),";
    }
    std::cout << std::endl;



    std::set<std::string> sOutPhone;
    sOutPhone.insert("111111111111");
    sOutPhone.insert("222222222222");
    
    std::cout << "getTokens: ";
    for (const auto& itVec : sOutPhone) {
        std::cout << "(" << itVec << "),";
    }
    std::cout << std::endl;

    contacts.clear();
    REQUIRE(pContact->getInParts(testUidMissed, vecPart, contacts) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);

    std::map<std::string, std::map<bcm::dao::FriendEventType, std::map<int64_t, std::string>>> alldata;
    std::string uids[] = {
            "1FZ9cDd6Mqq15m7QFwq5C1mkatyk1ujcJL",
            "13bYv8XkhgT8VjDrrekJDMR9QqeWWBdLiV",
            "1GtrnFtaasT1NWsUb5sJW945EP25diRyvw"
    };
    for (const auto& uid : uids) {
        int64_t index = 1;
        for (int i = 0; i < 200; i++) {
            std::string data = uid + "1" + std::to_string(index);
            alldata[uid][bcm::dao::FriendEventType::FRIEND_REQUEST][index] = data;
            index++;
        }

        for (int i = 0; i < 200; i++) {
            std::string data = uid + "2" + std::to_string(index);
            alldata[uid][bcm::dao::FriendEventType::FRIEND_REPLY][index] = data;
            index++;
        }

        for (int i = 0; i < 200; i++) {
            std::string data = uid + "3" + std::to_string(index);
            alldata[uid][bcm::dao::FriendEventType::DELETE_FRIEND][index] = data;
            index++;
        }
    }

    for (const auto& uid_type : alldata) {
        for (const auto& type_id : uid_type.second) {
            for (const auto& id_data : type_id.second) {
                int64_t id;
                int err = pContact->addFriendEvent(uid_type.first, type_id.first, id_data.second, id);
                REQUIRE(err == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
                REQUIRE(id == id_data.first);
            }
        }
    }
    for (const auto& uid_type : alldata) {
        for (const auto& type_id : uid_type.second) {
            for (int i = 0; i < 4; i++) {
                std::vector<bcm::dao::FriendEvent> result;
                int err = pContact->getFriendEvents(uid_type.first, type_id.first, 50, result);
                REQUIRE(err == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
                REQUIRE(result.size() == 50);
                std::vector<int64_t> ids;
                for (const auto& rec : result) {
                    REQUIRE(rec.data == type_id.second.at(rec.id));
                    ids.push_back(rec.id);
                }

                err = pContact->delFriendEvents(uid_type.first, type_id.first, ids);
                REQUIRE(err == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            }
        }
    }

    inKey["1"] = std::string(1024 * 100 + 1, 'k');
    REQUIRE(pContact->setInParts(testUid, inKey) == bcm::dao::ErrorCode::ERRORCODE_INVALID_ARGUMENT);
}

TEST_CASE("sign_up_challenge")
{
    initialize();

    std::shared_ptr<bcm::dao::SignUpChallenges> pSC = bcm::dao::ClientFactory::signupChallenges();
    bcm::SignUpChallenge challenge;
    challenge.set_difficulty(1);
    challenge.set_nonce(2);
    challenge.set_timestamp(123456);

    REQUIRE(pSC->set("uid1", challenge) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    bcm::SignUpChallenge tmp;
    REQUIRE(pSC->get("uid1", tmp) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(tmp.difficulty() == challenge.difficulty());
    REQUIRE(tmp.nonce() == challenge.nonce());
    REQUIRE(tmp.timestamp() == challenge.timestamp());

    REQUIRE(pSC->del("uid1") == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    REQUIRE(pSC->get("uid1", tmp) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
}

////////////////////////////////////////////////////////////////
///////////    account, messages                  //////////////

TEST_CASE("RpcMessageTest")
{
    initialize();

    bcm::StoredMessage msg;
    msg.set_msgtype(bcm::StoredMessage::MSGTYPE_PREKEY_BUNDLE);
    msg.set_destination("13500010014");
    msg.set_destinationdeviceid(1);
    msg.set_source("13500020002");
    msg.set_sourcedeviceid(1);
    msg.set_relay("dddddd");
    msg.set_content("messages content");
    msg.set_sourceextra("sourceextra");

    std::shared_ptr<bcm::dao::StoredMessages> m_msg = bcm::dao::ClientFactory::storedMessages();

    bcm::dao::ErrorCode  res;
    uint32_t unreadMsgCount;
    res = m_msg->set(msg, unreadMsgCount);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    msg.set_destinationregistrationid(9999);
    msg.set_sourceregistrationid(8888);
    msg.set_source("");

    res = m_msg->set(msg, unreadMsgCount);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    res = m_msg->set(msg, unreadMsgCount);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    // get all message
    std::vector<bcm::StoredMessage> msgs;
    res = m_msg->get("13500010014", 1, 100, msgs);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);
    REQUIRE((unreadMsgCount + 1) == msgs.size());

    for (auto itMsg : msgs) {
        REQUIRE(itMsg.sourceregistrationid() == 0);
        REQUIRE(itMsg.destinationregistrationid() == 0);
        REQUIRE(itMsg.source() == "13500020002");
        REQUIRE(itMsg.sourceextra() == "sourceextra");

        std::vector<uint64_t>   delMsgId;
        delMsgId.push_back(itMsg.id());
        res = m_msg->del(itMsg.destination(), delMsgId);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);
        break;
    }

    msgs.clear();
    res = m_msg->get("13500010014", 1, 5, msgs);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);
    REQUIRE(unreadMsgCount == msgs.size());

    std::vector<uint64_t>   delMsgId;
    for (auto& itMsg : msgs) {
        REQUIRE(itMsg.sourceregistrationid() == 8888);
        REQUIRE(itMsg.destinationregistrationid() == 9999);
        REQUIRE(itMsg.source() == "");
        REQUIRE(itMsg.sourceextra() == "sourceextra");

        delMsgId.push_back(itMsg.id());
    }
    res = m_msg->del("13500010014", delMsgId);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    //
    msgs.clear();
    res = m_msg->get("13500010014", 1, 100, msgs);
    REQUIRE(msgs.size() == 0);
    REQUIRE(bcm::dao::ERRORCODE_NO_SUCH_DATA == res);

    // begin test account
    res = m_msg->set(msg, unreadMsgCount);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    msg.set_destinationdeviceid(2);
    res = m_msg->set(msg, unreadMsgCount);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    msgs.clear();
    res = m_msg->get("13500010014", 2, 100, msgs);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    res = m_msg->clear(msg.destination(), 2);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    msgs.clear();
    res = m_msg->get("13500010014", 2, 100, msgs);
    REQUIRE(msgs.size() == 0);
    REQUIRE(bcm::dao::ERRORCODE_NO_SUCH_DATA == res);

    res = m_msg->clear(msg.destination());
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS == res);

    msgs.clear();
    res = m_msg->get("13500010014", 1, 100, msgs);
    REQUIRE(msgs.size() == 0);
    REQUIRE(bcm::dao::ERRORCODE_NO_SUCH_DATA == res);

    msg.set_content(std::string(1024 * 200 + 1, 'k'));
    REQUIRE(bcm::dao::ERRORCODE_INVALID_ARGUMENT == m_msg->set(msg, unreadMsgCount));
}


TEST_CASE("groups")
{
    initialize();

    std::shared_ptr<bcm::dao::Groups> pGroups = bcm::dao::ClientFactory::groups();
    REQUIRE(pGroups != nullptr);

    std::string sChnId = "dsfsda";

    bcm::Group newGroup;
    newGroup.set_gid(444444);
    newGroup.set_lastmid(3333);
    newGroup.set_name("group1");
    newGroup.set_icon("icon_path");
    newGroup.set_permission(1111);
    newGroup.set_updatetime(333333333);
    bcm::Group_BroadcastStatus  gBroadStatus = static_cast<bcm::Group_BroadcastStatus>(1);
    newGroup.set_broadcast(gBroadStatus);
    newGroup.set_status(1);
    newGroup.set_channel(sChnId);
    newGroup.set_intro("eeeeeeee");
    newGroup.set_createtime(2432413254);
    newGroup.set_key("ddfdsafdsafa");
    bcm::Group_EncryptStatus gEncryptStatus = static_cast<bcm::Group_EncryptStatus>(1);
    newGroup.set_encryptstatus(gEncryptStatus);
    newGroup.set_plainchannelkey("ddddd");
    newGroup.set_notice("{}");
    newGroup.set_shareqrcodesetting("shareqrcodesetting" + std::to_string(newGroup.gid()));
    newGroup.set_ownerconfirm(1);
    newGroup.set_sharesignature("sharesignature" + std::to_string(newGroup.gid()));
    newGroup.set_shareandownerconfirmsignature("shareandownerconfirmsignature" + std::to_string(newGroup.gid()));
    newGroup.set_version(1);
    newGroup.set_encryptedgroupinfosecret("encryptedgroupinfosecret" + std::to_string(newGroup.gid()));

    uint64_t gid;
    REQUIRE(pGroups->create(newGroup, gid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    newGroup.set_gid(gid);
    std::cout << "create_group_id: " << gid << std::endl;
    
    bcm::Group gidGroup;
    REQUIRE(pGroups->get(gid, gidGroup) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(gidGroup, newGroup) == true);
    
    std::cout << "get group: " << gidGroup.Utf8DebugString() << std::endl;

    nlohmann::json jsObj;
    jsObj["name"] = "group2";
    jsObj["encrypted_group_info_secret"] = "encryptedgroupinfosecret_updated";


    REQUIRE(pGroups->update(gid, jsObj) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);


    REQUIRE(pGroups->get(gid, gidGroup) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    if (gidGroup.name() != "group2") {
        std::cout << "get group jsonUpdate: " << jsObj.dump() << " ,group: " << gidGroup.Utf8DebugString() << std::endl;
    }
    REQUIRE(gidGroup.name() == "group2");
    REQUIRE(gidGroup.encryptedgroupinfosecret() == "encryptedgroupinfosecret_updated");
//    REQUIRE(gidGroup.role() == bcm::GroupUser::Role::GroupUser_Role_ROLE_ADMINISTROR);


    // begin group message
    bcm::GroupMsg  newMsg;
    newMsg.set_gid(gid);
    newMsg.set_fromuid("5555555");
    newMsg.set_text("dddfffff");
    newMsg.set_updatetime(44444444);
    newMsg.set_mid(0);
    newMsg.set_type(bcm::GroupMsg::TYPE_CHAT);
    newMsg.set_status(1);
    newMsg.set_atall(1);
    newMsg.set_atlist("ddddsss");
    newMsg.set_createtime(555555555);
    newMsg.set_sourceextra("source_extra" + newMsg.text());
    newMsg.set_verifysig("verify_sig" + newMsg.fromuid());

    std::shared_ptr<bcm::dao::GroupMsgs> pGroupMsg = bcm::dao::ClientFactory::groupMsgs();
    REQUIRE(pGroupMsg != nullptr);
    uint64_t newOutMid = 0;
    REQUIRE(pGroupMsg->insert(newMsg, newOutMid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    bcm::GroupMsg tmpMsg;
    REQUIRE(pGroupMsg->get(gid, newOutMid, tmpMsg) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(tmpMsg.fromuid() == newMsg.fromuid());
    // REQUIRE(tmpMsg.updatetime() == newMsg.updatetime());
    REQUIRE(tmpMsg.text() == newMsg.text());
    REQUIRE(tmpMsg.type() == newMsg.type());
    REQUIRE(tmpMsg.atlist() == newMsg.atlist());
    REQUIRE(tmpMsg.status() == newMsg.status());
    REQUIRE(tmpMsg.sourceextra() == newMsg.sourceextra());
    REQUIRE(tmpMsg.verifysig() == newMsg.verifysig());
    // REQUIRE(tmpMsg.createtime() == newMsg.createtime());

    std::vector<bcm::GroupMsg>  msgs;
    REQUIRE(pGroupMsg->batchGet(gid, 0, 0, 50, bcm::GroupUser::ROLE_ADMINISTROR, true, msgs) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(msgs.size() > 0);

    for (const auto& itMsg : msgs) {
        uint64_t groupId = itMsg.gid();
        uint64_t mid = itMsg.mid();
        bcm::GroupMsg tmpMsg;
        REQUIRE(pGroupMsg->get(groupId, mid, tmpMsg) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(tmpMsg.text() == itMsg.text());
        REQUIRE(tmpMsg.text() == "dddfffff");

    }

    REQUIRE(pGroups->del(gid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    newMsg.set_text(std::string(1024 * 200 + 1, 'k'));
    REQUIRE(bcm::dao::ErrorCode::ERRORCODE_INVALID_ARGUMENT == pGroupMsg->insert(newMsg, newOutMid));
}

TEST_CASE("group_users")
{
    initialize();

    std::shared_ptr<bcm::dao::GroupUsers> pgu = bcm::dao::ClientFactory::groupUsers();
    REQUIRE(pgu != nullptr);
    std::shared_ptr<bcm::dao::Groups> pGroups = bcm::dao::ClientFactory::groups();
    REQUIRE(pGroups != nullptr);

    bcm::Group newGroup, newGroup1;
    newGroup.set_gid(444444);
    newGroup.set_lastmid(3333);
    newGroup.set_name("group1");
    newGroup.set_icon("icon_path");
    newGroup.set_permission(1111);
    newGroup.set_updatetime(333333333);
    bcm::Group_BroadcastStatus  gBroadStatus = static_cast<bcm::Group_BroadcastStatus>(1);
    newGroup.set_broadcast(gBroadStatus);
    newGroup.set_status(1);
    newGroup.set_channel("channel");
    newGroup.set_intro("eeeeeeee");
    newGroup.set_createtime(2432413254);
    newGroup.set_key("ddfdsafdsafa");
    bcm::Group_EncryptStatus gEncryptStatus = static_cast<bcm::Group_EncryptStatus>(1);
    newGroup.set_encryptstatus(gEncryptStatus);
    newGroup.set_plainchannelkey("ddddd");
    newGroup.set_notice("{}");
    newGroup.set_shareqrcodesetting("shareqrcodesetting" + std::to_string(newGroup.gid()));
    newGroup.set_ownerconfirm(1);
    newGroup.set_sharesignature("sharesignature" + std::to_string(newGroup.gid()));
    newGroup.set_shareandownerconfirmsignature("shareandownerconfirmsignature" + std::to_string(newGroup.gid()));
    newGroup.set_version(0);
    newGroup.set_encryptedgroupinfosecret("encryptedgroupinfosecret" + std::to_string(newGroup.gid()));

    uint64_t gid, gid1;
    REQUIRE(pGroups->create(newGroup, gid) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    std::cout << "create_group_id1: " << gid << std::endl;
    newGroup1.set_gid(444444);
    newGroup1.set_lastmid(444);
    newGroup1.set_name("group2");
    newGroup1.set_icon("icon_path2");
    newGroup1.set_permission(11112);
    newGroup1.set_updatetime(3333333332);
    gBroadStatus = static_cast<bcm::Group_BroadcastStatus>(0);
    newGroup1.set_broadcast(gBroadStatus);
    newGroup1.set_status(0);
    newGroup1.set_channel("channel1");
    newGroup1.set_intro("eeeeeeee1");
    newGroup1.set_createtime(24324132541);
    newGroup1.set_key("ddfdsafdsafa1");
    gEncryptStatus = static_cast<bcm::Group_EncryptStatus>(0);
    newGroup1.set_encryptstatus(gEncryptStatus);
    newGroup1.set_plainchannelkey("ddddd1");
    newGroup1.set_notice("{}");
    newGroup1.set_shareqrcodesetting("shareqrcodesetting" + std::to_string(newGroup.gid()));
    newGroup1.set_ownerconfirm(1);
    newGroup1.set_sharesignature("sharesignature" + std::to_string(newGroup.gid()));
    newGroup1.set_shareandownerconfirmsignature("shareandownerconfirmsignature" + std::to_string(newGroup.gid()));
    newGroup1.set_version(1);
    newGroup1.set_encryptedgroupinfosecret("encryptedgroupinfosecret" + std::to_string(newGroup1.gid()));

    REQUIRE(pGroups->create(newGroup1, gid1) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    std::cout << "create_group_id2: " << gid1 << std::endl;
    std::map<uint64_t, bcm::Group> newGroups;
    newGroup.set_gid(gid);
    newGroup1.set_gid(gid1);
    newGroups[gid] = newGroup;
    newGroups[gid1] = newGroup1;

    ::bcm::GroupUser gu;
    gu.set_gid(gid);
    gu.set_uid("uid123456789");
    gu.set_nick(gu.uid());
    gu.set_updatetime(123456789);
    gu.set_lastackmid(0);
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER);
    gu.set_status(0);
    gu.set_encryptedkey("encryptedkey");
    gu.set_createtime(123456789);
    gu.set_nickname("dddddddddd");
    gu.set_profilekeys("eeeeeeeeeee");
    gu.set_groupnickname(gu.uid());
    gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());

    REQUIRE(pgu->insert(gu) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    bcm::GroupUser tmp;
    REQUIRE(pgu->getMember(gu.gid(), gu.uid(), tmp) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    std::map<uint64_t, std::map<std::string, bcm::GroupUser>> gus;
    std::vector<bcm::GroupUser> tmps;
    gus[gu.gid()][gu.uid()] = gu;
    gu.set_uid("uid223456789");
    gu.set_nick(gu.uid());
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
    gu.set_groupnickname(gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());
    gus[gu.gid()][gu.uid()] = gu;
    tmps.push_back(gu);
    gu.set_uid("uid323456789");
    gu.set_nick(gu.uid());
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_SUBSCRIBER);
    gu.set_groupnickname(gu.uid());
    gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());
    gus[gu.gid()][gu.uid()] = gu;
    tmps.push_back(gu);
    REQUIRE(pgu->insertBatch(tmps) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    tmps.clear();
    gu.set_gid(gid1);
    gu.set_uid("uid123456789");
    gu.set_nick(gu.uid());
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER);
    gu.set_groupnickname(gu.uid());
    gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());

    gus[gu.gid()][gu.uid()] = gu;
    tmps.push_back(gu);
    gu.set_uid("uid223456789");
    gu.set_nick(gu.uid());
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
    gu.set_groupnickname(gu.uid());
    gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());

    gus[gu.gid()][gu.uid()] = gu;
    tmps.push_back(gu);
    gu.set_uid("uid323456789");
    gu.set_nick(gu.uid());
    gu.set_role(bcm::GroupUser::Role::GroupUser_Role_ROLE_SUBSCRIBER);
    gu.set_groupnickname(gu.uid());
    gu.set_groupinfosecret("groupinfosecret" + std::to_string(gu.gid()) + "_" + gu.uid());
    gu.set_proof("proof" + std::to_string(gu.gid()) + "_" + gu.uid());

    gus[gu.gid()][gu.uid()] = gu;
    tmps.push_back(gu);
    REQUIRE(pgu->insertBatch(tmps) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    ::bcm::GroupUser::Role role;
    REQUIRE(pgu->getMemberRole(gu.gid(), gu.uid(), role) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(gu.role() == role);

    std::map <std::string, bcm::GroupUser::Role> userRoles;
    userRoles.emplace("uid123456789", bcm::GroupUser::Role::GroupUser_Role_ROLE_UNDEFINE);
    userRoles.emplace("uid223456789", bcm::GroupUser::Role::GroupUser_Role_ROLE_UNDEFINE);
    userRoles.emplace("uid323456789", bcm::GroupUser::Role::GroupUser_Role_ROLE_UNDEFINE);
    REQUIRE(pgu->getMemberRoles(gu.gid(), userRoles) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(userRoles["uid123456789"] == bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER);
    REQUIRE(userRoles["uid223456789"] == bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);
    REQUIRE(userRoles["uid323456789"] == bcm::GroupUser::Role::GroupUser_Role_ROLE_SUBSCRIBER);

    std::vector<bcm::GroupUser> users;
    REQUIRE(pgu->getMemberBatch(gu.gid(), {"uid123456789", "uid223456789"}, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupUsersEquals(users[0], gus[users[0].gid()][users[0].uid()]) == true);
    REQUIRE(groupUsersEquals(users[1], gus[users[1].gid()][users[1].uid()]) == true);

    users.clear();
    REQUIRE(pgu->getMemberRangeByRolesBatch(gu.gid(), {bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER, bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER}, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupUsersEquals(users[0], gus[users[0].gid()][users[0].uid()]) == true);
    REQUIRE(groupUsersEquals(users[1], gus[users[1].gid()][users[1].uid()]) == true);

    users.clear();
    REQUIRE(pgu->getMemberRangeByRolesBatchWithOffset(gu.gid(), {bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER, bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER}, "", 1, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(users.size() == 1);
    REQUIRE(groupUsersEquals(users[0], gus[users[0].gid()][users[0].uid()]) == true);
    std::string startUid = users[0].uid();
    users.clear();
    REQUIRE(pgu->getMemberRangeByRolesBatchWithOffset(gu.gid(), {bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER, bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER}, startUid, 1, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
 
    users.clear();
    REQUIRE(pgu->getMembersOrderByCreateTime(gu.gid(), {bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER, bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER}, "", 0, 1, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(users.size() == 1);
    REQUIRE(groupUsersEquals(users[0], gus[users[0].gid()][users[0].uid()]) == true);
    startUid = users[0].uid();
    int64_t createTime = users[0].createtime();
    users.clear();
    REQUIRE(pgu->getMembersOrderByCreateTime(gu.gid(), {bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER, bcm::GroupUser::Role::GroupUser_Role_ROLE_OWNER}, startUid, createTime, 1, users) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(users.size() == 1);
    REQUIRE(groupUsersEquals(users[0], gus[users[0].gid()][users[0].uid()]) == true);

    std::vector <::bcm::dao::UserGroupDetail> groups;
    REQUIRE(pgu->getJoinedGroupsList(gu.uid(), groups) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(groups[0].group, newGroups[groups[0].group.gid()]) == true);
    REQUIRE(groupUsersEquals(groups[0].user, gus[groups[0].user.gid()][gu.uid()]) == true);
    REQUIRE(groups[0].counter.owner == "uid123456789");
    REQUIRE(groups[0].counter.memberCnt == 2);
    REQUIRE(groups[0].counter.subscriberCnt == 1);

    REQUIRE(groupEquals(groups[1].group, newGroups[groups[1].group.gid()]) == true);
    REQUIRE(groupUsersEquals(groups[1].user, gus[groups[1].user.gid()][gu.uid()]) == true);
    REQUIRE(groups[1].counter.owner == "uid123456789");
    REQUIRE(groups[1].counter.memberCnt == 2);
    REQUIRE(groups[1].counter.subscriberCnt == 1);

    std::vector <uint64_t> gids;
    REQUIRE(pgu->getJoinedGroups(gu.uid(), gids) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE((gids[0] == gid || gids[0] == gid1));
    REQUIRE((gids[1] == gid1 || gids[1] == gid));
    REQUIRE(gids[0] != gids[1]);

    ::bcm::dao::UserGroupDetail detail;
    REQUIRE(pgu->getGroupDetailByGid(gu.gid(), gu.uid(), detail) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(detail.group, newGroups[gu.gid()]) == true);
    REQUIRE(groupUsersEquals(detail.user, gus[gu.gid()][gu.uid()]) == true);
    REQUIRE(detail.counter.owner == "uid123456789");
    REQUIRE(detail.counter.memberCnt == 2);
    REQUIRE(detail.counter.subscriberCnt == 1);

    REQUIRE(pgu->getGroupDetailByGid(gu.gid(), "", detail) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(detail.group, newGroups[gu.gid()]) == true);
    REQUIRE(detail.counter.owner == "uid123456789");
    REQUIRE(detail.counter.memberCnt == 2);
    REQUIRE(detail.counter.subscriberCnt == 1);

    REQUIRE(pgu->getGroupDetailByGid(gu.gid(), "not_exists", detail) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(detail.group, newGroups[gu.gid()]) == true);
    REQUIRE(detail.counter.owner == "uid123456789");
    REQUIRE(detail.counter.memberCnt == 2);
    REQUIRE(detail.counter.subscriberCnt == 1);

    std::vector<::bcm::dao::UserGroupEntry> details;
    REQUIRE(pgu->getGroupDetailByGidBatch({gid, gid1}, gu.uid(), details) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(details[0].group, newGroups[details[0].group.gid()]) == true);
    REQUIRE(groupUsersEquals(details[0].user, gus[details[0].group.gid()][gu.uid()]) == true);
    REQUIRE(details[0].owner == "uid123456789");
    REQUIRE(groupEquals(details[1].group, newGroups[details[1].group.gid()]) == true);
    REQUIRE(groupUsersEquals(details[1].user, gus[details[1].group.gid()][gu.uid()]) == true);
    REQUIRE(details[1].owner == "uid123456789");
    details.clear();
    REQUIRE(pgu->getGroupDetailByGidBatch({gid, gid1}, "", details) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(details[0].group, newGroups[details[0].group.gid()]) == true);
    REQUIRE(details[0].owner == "uid123456789");
    REQUIRE(groupEquals(details[1].group, newGroups[details[1].group.gid()]) == true);
    REQUIRE(details[1].owner == "uid123456789");
    details.clear();
    REQUIRE(pgu->getGroupDetailByGidBatch({gid, gid1}, "not_exists", details) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(groupEquals(details[0].group, newGroups[details[0].group.gid()]) == true);
    REQUIRE(details[0].owner == "uid123456789");
    REQUIRE(groupEquals(details[1].group, newGroups[details[1].group.gid()]) == true);
    REQUIRE(details[1].owner == "uid123456789");

    std::string owner;
    REQUIRE(pgu->getGroupOwner(gid, owner) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(owner == "uid123456789");

    ::bcm::dao::GroupCounter counter;
    REQUIRE(pgu->queryGroupMemberInfoByGid(gid, counter) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(counter.owner == "uid123456789");
    REQUIRE(counter.memberCnt == 2);
    REQUIRE(counter.subscriberCnt == 1);

    bcm::GroupUser::Role role1;
    REQUIRE(pgu->queryGroupMemberInfoByGid(gid, counter, gu.uid(), role, "uid223456789", role1) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(counter.owner == "uid123456789");
    REQUIRE(counter.memberCnt == 2);
    REQUIRE(counter.subscriberCnt == 1);
    REQUIRE(role == bcm::GroupUser::Role::GroupUser_Role_ROLE_SUBSCRIBER);
    REQUIRE(role1 == bcm::GroupUser::Role::GroupUser_Role_ROLE_MEMBER);

    std::map<std::string, std::string> updated;
    updated["nick"] = "updated nick";
    updated["status"] = "1";
    updated["nick_name"] = "333333";
    updated["profile_keys"] = "555555";
    updated["group_nick_name"] = "group_nick_name_updated";
    updated["group_info_secret"] = "group_info_secret_updated";

    REQUIRE(pgu->update(gu.gid(), gu.uid(), nlohmann::json(updated)) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(pgu->getMember(gu.gid(), gu.uid(), tmp) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(tmp.nick() == updated["nick"]);
    REQUIRE(tmp.nickname() == updated["nick_name"]);
    REQUIRE(tmp.profilekeys() == updated["profile_keys"]);
    REQUIRE(tmp.groupnickname() == updated["group_nick_name"]);
    REQUIRE(tmp.groupinfosecret() == updated["group_info_secret"]);
    REQUIRE(tmp.status() == 1);


    REQUIRE(pgu->delMember(gu.gid(), gu.uid()) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(pgu->getMember(gu.gid(), gu.uid(), tmp) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    REQUIRE(pgu->delMemberBatch(gu.gid(), {"uid123456789", "uid223456789"}) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(pgu->getMember(gu.gid(), "uid123456789", tmp) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    REQUIRE(pgu->getMember(gu.gid(), "uid223456789", tmp) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    
}

TEST_CASE("limiters")
{
    initialize();

    std::shared_ptr<bcm::dao::Limiters> pl = bcm::dao::ClientFactory::limiters();
    bcm::Limiter limiter;
    limiter.set_allowance(1.234567890123456);
    limiter.set_capacity(5);
    limiter.set_rate(12.345678901234567);
    limiter.set_timestamp(123456);
    std::map<std::string, bcm::Limiter> inserted;
    inserted.emplace("13800138000", limiter);
    REQUIRE(pl->setLimiters(inserted) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);

    std::map<std::string, bcm::Limiter> getResults;
    REQUIRE(pl->getLimiters({"13800138000"}, getResults) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    REQUIRE(getResults.size() == 1);
    REQUIRE(getResults.begin()->first == "13800138000");
    REQUIRE(getResults.begin()->second.allowance() == 1.234567890123456);
    REQUIRE(getResults.begin()->second.capacity() == 5);
    REQUIRE(getResults.begin()->second.rate() == 12.345678901234567);
    REQUIRE(getResults.begin()->second.timestamp() == 123456);

}

TEST_CASE("SysMsgs")
{
    initialize();

    std::shared_ptr<bcm::dao::SysMsgs> pl = bcm::dao::ClientFactory::sysMsgs();

    uint64_t mMsgStart = 200000;
    std::string descId = "13800138000";
    uint32_t kMaxSysMsgSize = 16;


    {
        pl->delBatch(descId, mMsgStart + 1000);
    }

    for (uint32_t i = 0; i < 100; i++) {
        bcm::SysMsg  mSysMsg;
        mSysMsg.set_destination(descId);
        mSysMsg.set_sysmsgid(mMsgStart + i);
        mSysMsg.set_content(std::to_string(mMsgStart + i));

        REQUIRE(pl->insert(mSysMsg) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    uint32_t i = 0;

    {  // del()
        std::vector<bcm::SysMsg>  mMsgList;
        REQUIRE(pl->get(descId, mMsgList) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(mMsgList.size() == kMaxSysMsgSize);

        for (const auto& itMsg : mMsgList) {
            REQUIRE(itMsg.sysmsgid()  == (mMsgStart + i));
            REQUIRE(pl->del(descId, itMsg.sysmsgid()) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            i++;
        }
    }

    {  // delbatch

        std::vector<bcm::SysMsg>  mMsgList;
        REQUIRE(pl->get(descId, mMsgList) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(mMsgList.size() == kMaxSysMsgSize);

        std::vector<uint64_t>  delMsgIds;

        for (const auto& itMsg : mMsgList) {
            REQUIRE(itMsg.sysmsgid()  == (mMsgStart + i));
            delMsgIds.push_back(itMsg.sysmsgid());
            i++;
        }

        REQUIRE(pl->delBatch(descId, delMsgIds) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    { // delBatch  clean
        std::vector<bcm::SysMsg>  mMsgList;
        REQUIRE(pl->get(descId, mMsgList) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(mMsgList.size() == kMaxSysMsgSize);

        uint64_t  maxDelMsgId = 0;

        for (const auto& itMsg : mMsgList) {
            REQUIRE(itMsg.sysmsgid()  == (mMsgStart + i));
            maxDelMsgId = itMsg.sysmsgid();
            i++;
        }

        REQUIRE(pl->delBatch(descId, maxDelMsgId) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
    }

    { // check
        std::vector<bcm::SysMsg>  mMsgList;
        REQUIRE(pl->get(descId, mMsgList) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(mMsgList.size() == kMaxSysMsgSize);

        for (const auto& itMsg : mMsgList) {
            REQUIRE(itMsg.sysmsgid()  == (mMsgStart + i));
            i++;
        }
    }

}

// The following tests use thread to simulate two process to compete to get lease.
// The process that first gets lease can call renew lease to renew the lease continuously.
// Another process must wait until the process of first acquiring lease to
// abandon lease or lease expired in order to successfully obtain lease
TEST_CASE("MasterLease")
{
    initialize();

    std::shared_ptr<bcm::dao::MasterLease> ml = bcm::dao::ClientFactory::masterLease();
    std::string key = "abcd_xxoo_1";
    uint32_t ttl_ms = 3000;

    // To simulate one of process
    std::thread participant1([ml, key, ttl_ms]() {
        bcm::dao::ErrorCode ec = ml->getLease(key, ttl_ms);
        std::cout << "thread1 getLease ret: " << std::to_string(ec) << std::endl;
        sleep(1);

        // Renew lease
        if (bcm::dao::ERRORCODE_SUCCESS == ec) {
            bcm::dao::ErrorCode ec = ml->renewLease(key, ttl_ms);
            std::cout << "thread1 renewLease ret: " << std::to_string(ec) << std::endl;
        }
    });

    // Make sure participant1 has got lease
    sleep(1);

    // To simulate another process
    std::thread participant2([ml, key, ttl_ms]() {
        for (size_t i=0; i<20; i++) {
            bcm::dao::ErrorCode ec = ml->getLease(key, ttl_ms);
            std::cout << "thread2 getLease ret: " << std::to_string(ec) << std::endl;
            sleep(1);

            if (bcm::dao::ERRORCODE_SUCCESS == ec) {
                bcm::dao::ErrorCode ec = ml->renewLease(key, ttl_ms);
                std::cout << "thread2 renewLease ret: " << std::to_string(ec) << std::endl;
                sleep(2);
            }
        }
    });

    participant1.join();
    participant2.join();
}

TEST_CASE("ThreadCancel") {
    std::thread th1([] () {
        for (size_t i=0; i<20; i++) {
            sleep(1);
        }
    });
    std::cout << "native handle: " << th1.native_handle() << std::endl;
    th1.detach();
    std::cout << "native handle: " << th1.native_handle() << std::endl;
}
TEST_CASE("pending_group_users")
{
    initialize();
    std::map<uint64_t, std::vector<bcm::PendingGroupUser>> allData;
    std::shared_ptr<bcm::dao::PendingGroupUsers> pl = bcm::dao::ClientFactory::pendingGroupUsers();
    for (uint64_t gid = 1; gid < 10; gid++) {
        for (int i = 0; i < 100; i++) {
            bcm::PendingGroupUser pgu;
            pgu.set_gid(gid);
            char buf[12] = {0};
            snprintf(buf, 12, "%011" PRId32, i);
            pgu.set_uid("uid" + std::string(buf));
            pgu.set_inviter("inviter" + std::to_string(gid) + pgu.uid());
            pgu.set_signature("signature" + std::to_string(gid) + pgu.uid());
            pgu.set_comment("comment" + std::to_string(gid) + pgu.uid());
            REQUIRE(pl->set(pgu) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            allData[gid].emplace_back(pgu);
        }
    }

    for (const auto& item : allData) {
        std::string startUid = "";
        for (int i = 0; i < 10; i++) {
            std::vector<bcm::PendingGroupUser> result;
            REQUIRE(pl->query(item.first, startUid, 10, result) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            REQUIRE(result.size() == 10);
            for (int j = 0; j < 10; j++) {
                REQUIRE(pendingGroupUserEquals(item.second.at(i * 10 + j), result[j]));
            }
            startUid = result.back().uid();
        }
    }

    for (const auto& item : allData) {
        std::vector<bcm::PendingGroupUser> result;
        REQUIRE(pl->del(item.first, {item.second.front().uid()}) == bcm::dao::ERRORCODE_SUCCESS);
        REQUIRE(pl->query(item.first, item.second.front().uid(), 100, result) == bcm::dao::ERRORCODE_SUCCESS);
        REQUIRE(result.size() == 99);
        for (const auto& it : result) {
            REQUIRE(it.uid() != item.second.front().uid());
        }
    }

    for (const auto& item : allData) {
        std::vector<bcm::PendingGroupUser> result;
        REQUIRE(pl->clear(item.first) == bcm::dao::ERRORCODE_SUCCESS);
        REQUIRE(pl->query(item.first, "", 1, result) == bcm::dao::ERRORCODE_NO_SUCH_DATA);
    }
}

void compareKey(const bcm::GroupKeys& lhs, const bcm::GroupKeys& rhs)
{
    REQUIRE(lhs.gid() == rhs.gid());
    REQUIRE(lhs.version() == rhs.version());
    REQUIRE(lhs.groupowner() == rhs.groupowner());
    REQUIRE(lhs.groupkeys() == rhs.groupkeys());
    REQUIRE(lhs.mode() == rhs.mode());
    REQUIRE(lhs.creator() == rhs.creator());
    REQUIRE(lhs.createtime() == rhs.createtime());
}

TEST_CASE("group_message_key")
{
    initialize();
    std::map<uint64_t, std::map<int, bcm::GroupKeys>> allData;
    std::shared_ptr<bcm::dao::GroupKeys> gk = bcm::dao::ClientFactory::groupKeys();
    for (uint64_t gid = 1; gid < 10; gid++) {
        for (int i = 0; i < 100; i++) {
            bcm::GroupKeys key;
            key.set_gid(gid);
            key.set_version(i);
            key.set_groupowner("owner" + std::to_string(i));
            key.set_groupkeys(std::to_string(gid) + std::to_string(i) + key.groupowner());
            key.set_mode(static_cast<bcm::GroupKeys::GroupKeysMode>(i % bcm::GroupKeys::GroupKeysMode_ARRAYSIZE));
            key.set_creator(key.groupowner());
            key.set_createtime(i);
            allData[gid][i] = key;
            REQUIRE(gk->insert(key) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            bcm::GroupKeys::GroupKeysMode mode;
            REQUIRE(gk->getLatestMode(gid, mode) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            REQUIRE(mode == key.mode());
        }
    }

    // get
    for (const auto& gid_keys : allData) {
        std::set<int64_t> versions;
        std::set<int64_t> noneCachedVersions;
        for (const auto& keys : gid_keys.second) {
            versions.emplace(keys.second.version());
            if (noneCachedVersions.size() % 2 == 0) {
                noneCachedVersions.emplace(keys.second.version());
            }
        }

        std::vector<bcm::GroupKeys> result;
        REQUIRE(gk->get(gid_keys.first, versions, result) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(result.size() == gid_keys.second.size());
        for (uint32_t i = 0; i < result.size(); i++) {
            compareKey(result.at(i), gid_keys.second.at(result.at(i).version()));
        }

        for (const auto& v : noneCachedVersions) {
            auto* cache = bcm::dao::GroupKeysRpcImpl::CacheManager::getInstance();
            cache->m_caches.m_caches.erase(cache->cacheKey(gid_keys.first, v));
        }
        result.clear();
        REQUIRE(gk->get(gid_keys.first, versions, result) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        REQUIRE(result.size() == gid_keys.second.size());
        for (uint32_t i = 0; i < result.size(); i++) {
            compareKey(result.at(i), gid_keys.second.at(result.at(i).version()));
        }
    }

    bcm::dao::GroupKeysRpcImpl::CacheManager::getInstance()->setBypass(true);
    // clear
    for (const auto& gid_keys : allData) {
        std::set<int64_t> versions;
        for (const auto& keys : gid_keys.second) {
            versions.emplace(keys.second.version());
        }
        REQUIRE(gk->clear(gid_keys.first) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
        std::vector<bcm::GroupKeys> result;
        REQUIRE(gk->get(gid_keys.first, versions, result) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
    }
}

void compareQrCodeGroupUser(const bcm::QrCodeGroupUser& lhs,
                            const bcm::QrCodeGroupUser& rhs)
{
    REQUIRE(lhs.gid() == rhs.gid());
    REQUIRE(lhs.uid() == rhs.uid());
}

TEST_CASE("qr_code_group_user")
{
    initialize();
    std::map<uint64_t, std::map<std::string, bcm::QrCodeGroupUser>> allData;
    std::shared_ptr<bcm::dao::QrCodeGroupUsers> qrCodeGroupUsers = bcm::dao::ClientFactory::qrCodeGroupUsers();
    int64_t ttl = 10;
    for (uint64_t gid = 1; gid < 10; gid++) {
        for (int i = 0; i < 100; i++) {
            std::string uid = "uid" + std::to_string(i);
            bcm::QrCodeGroupUser user;
            allData[gid][uid] = user;
            REQUIRE(qrCodeGroupUsers->set(user, ttl) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            bcm::QrCodeGroupUser obj;
            REQUIRE(qrCodeGroupUsers->get(user.gid(), user.uid(), obj) == bcm::dao::ErrorCode::ERRORCODE_SUCCESS);
            compareQrCodeGroupUser(user, obj);
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(ttl + 1));
    // get
    for (const auto& gid_uids : allData) {
        for (const auto& u : gid_uids.second) {
            bcm::QrCodeGroupUser obj;
            REQUIRE(qrCodeGroupUsers->get(u.second.gid(), u.second.uid(), obj) == bcm::dao::ErrorCode::ERRORCODE_NO_SUCH_DATA);
        }
    }
}



