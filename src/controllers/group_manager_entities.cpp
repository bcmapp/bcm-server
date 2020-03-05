#include "group_manager_entities.h"
#include "proto/dao/group.pb.h"
#include "proto/dao/group_keys.pb.h"
#include "utils/json_serializer.h"
#include "utils/log.h"

namespace bcm {

bool CreateGroupBodyV2::check(GroupResponse& response) {
    if (name.length() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max name length: " + std::to_string(kMaxGroupNameLength);
        return false;
    }

    if (icon.length() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max icon length: " + std::to_string(kMaxGroupIconLength);
        return false;
    }

    if (intro.length() > kMaxGroupIntroLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max intro length: " + std::to_string(kMaxGroupIntroLength);
        return false;

    }

    if (broadcast != static_cast<int32_t>(Group::BROADCAST_OFF)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "broadcast should be off";
        return false;
    }

    if (ownerKey.length() > kMaxGroupKeyLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max owner_key length: " + std::to_string(kMaxGroupKeyLength);
        return false;
    }

    if (members.size() > kMaxGroupCreateMemberCount) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max members count: " + std::to_string(kMaxGroupCreateMemberCount);
        return false;
    }

    if (members.size() == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "members is empty";
        return false;
    }

    std::set<std::string> memberSet;
    for (auto it = members.begin(); it != members.end(); ++it) {
        if (it->length() == 0 || it->length() > kMaxGroupUidLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "invalid member";
            return false;
        }
        if (memberSet.find(*it) == memberSet.end()) {
            memberSet.insert(*it);
        }
    }

    if (ownerKey.empty() || ownerSecretKey.empty()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "owner key can not be empty";
        return false;
    }

    if (members.size() != memberKeys.size() || members.size() != membersGroupInfoSecrets.size() ) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "secrets count is not equal to members count";
        return false;
    }
    for (const auto& it : memberKeys) {
        if (it.length() > kMaxGroupKeyLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "max memberKey length: " + std::to_string(kMaxGroupKeyLength);
            return false;
        }
    }
    for (const auto& it : membersGroupInfoSecrets) {
        if (it.length() > kMaxGroupInfoSecretLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "the length of group info secret beyond " + std::to_string(kMaxGroupInfoSecretLength);
            return false;
        }
    }
    
    return true;
}

bool UpdateGroupInfoBody::check(GroupResponse& response)
{
    if (name.empty() && icon.empty() && intro.empty() && broadcast == -1 && plainChannelKey.empty()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "content empty";
        return false;
    }

    if (name.length() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max name length: " + std::to_string(kMaxGroupNameLength);
        return false;
    }

    if (icon.length() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max icon length: " + std::to_string(kMaxGroupIconLength);
        return false;
    }

    if (intro.length() > kMaxGroupIntroLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max intro length: " + std::to_string(kMaxGroupIntroLength);
        return false;

    }

    if (broadcast != -1 && !Group::BroadcastStatus_IsValid(broadcast)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid broadcast setting";
        return false;
    } else if (broadcast == static_cast<int32_t>(Group::BROADCAST_OFF)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "broadcast cannot be turn off";
        return false;
    }

    if (plainChannelKey.length() > kMaxGroupPlainChannelKeyLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max plain_channel_key length: " + std::to_string(kMaxGroupPlainChannelKeyLength);
        return false;
    }

    return true;
}

bool UpdateGroupInfoBodyV2::check(GroupResponse& response)
{
    /*if (name.empty() && icon.empty() && intro.empty() && broadcast == -1 && plainChannelKey.empty()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "content empty";
        return false;
    }*/

    if (name.length() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max name length: " + std::to_string(kMaxGroupNameLength);
        return false;
    }

    if (icon.length() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max icon length: " + std::to_string(kMaxGroupIconLength);
        return false;
    }

    if (intro.length() > kMaxGroupIntroLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max intro length: " + std::to_string(kMaxGroupIntroLength);
        return false;

    }

    if (broadcast != -1 && !Group::BroadcastStatus_IsValid(broadcast)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid broadcast setting";
        return false;
    } else if (broadcast == static_cast<int32_t>(Group::BROADCAST_OFF)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "broadcast can not be turn off";
        return false;
    }

    if (plainChannelKey.length() > kMaxGroupPlainChannelKeyLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max plain_channel_key length: " + std::to_string(kMaxGroupPlainChannelKeyLength);
        return false;
    }

    if (shareSignature.size() > kMaxShareSignatureLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of shareSignature beyond" + std::to_string(kMaxShareSignatureLength);
        return false;
    }

    if (shareAndOwnerConfirmSignature.size() > kMaxShareAndOwnerConfirmSignatureLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of shareAndConfirmSignature beyond" + std::to_string(kMaxShareAndOwnerConfirmSignatureLength);
        return false;
    }

    if (qrCodeSetting.size() > kMaxQrCodeSettingLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of qrCodeSetting beyond" + std::to_string(kMaxQrCodeSettingLength);
        return false;
    }

    if (encryptedName.size() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_name length: " + std::to_string(encryptedName.size());
        return false;
    }

    if (encryptedIcon.size() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_icon length: " + std::to_string(encryptedIcon.size());
        return false;
    }

    if (encryptedNotice.size() > kMaxGroupNoticeLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_notice length: " + std::to_string(encryptedNotice.size());
        return false;
    }

    // set encrypted one only, should be delete after remove plain fields
    if (encryptedNameExisted || encryptedIconExisted || encryptedNoticeExisted) {
        if (nameExisted || iconExisted) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "please set encrypted only";
            return false;
        }
    }

    return true;
}

bool UpdateGroupInfoBodyV3::check(GroupResponse& response)
{
    /*if (name.empty() && icon.empty() && intro.empty() && broadcast == -1 && plainChannelKey.empty()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "content empty";
        return false;
    }*/

    if (name.length() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max name length: " + std::to_string(kMaxGroupNameLength);
        return false;
    }

    if (icon.length() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max icon length: " + std::to_string(kMaxGroupIconLength);
        return false;
    }

    if (intro.length() > kMaxGroupIntroLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max intro length: " + std::to_string(kMaxGroupIntroLength);
        return false;
    }

    if (broadcast != -1 && !Group::BroadcastStatus_IsValid(broadcast)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid broadcast setting";
        return false;
    } else if (broadcast == static_cast<int32_t>(Group::BROADCAST_OFF)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "broadcast can not be turn off";
        return false;
    }

    if (plainChannelKey.length() > kMaxGroupPlainChannelKeyLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max plain_channel_key length: " + std::to_string(kMaxGroupPlainChannelKeyLength);
        return false;
    }

    if (shareSignature.size() > kMaxShareSignatureLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of shareSignature beyond" + std::to_string(kMaxShareSignatureLength);
        return false;
    }

    if (shareAndOwnerConfirmSignature.size() > kMaxShareAndOwnerConfirmSignatureLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of shareAndConfirmSignature beyond" + std::to_string(kMaxShareAndOwnerConfirmSignatureLength);
        return false;
    }

    if (qrCodeSetting.size() > kMaxQrCodeSettingLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "the length of qrCodeSetting beyond" + std::to_string(kMaxQrCodeSettingLength);
        return false;
    }

    if (encryptedGroupInfoSecret.size() > kMaxGroupInfoSecretLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_group_info_secret length: " + std::to_string(encryptedGroupInfoSecret.size());
        return false;
    }   

    if (encryptedEphemeralKey.size() > kMaxGroupKeyLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_ephemeral_key length: " + std::to_string(encryptedEphemeralKey.size());
        return false;
    }

    if (encryptedName.size() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_name length: " + std::to_string(encryptedName.size());
        return false;
    }

    if (encryptedIcon.size() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_icon length: " + std::to_string(encryptedIcon.size());
        return false;
    }

    if (encryptedNotice.size() > kMaxGroupNoticeLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "invalid encrypted_notice length: " + std::to_string(encryptedNotice.size());
        return false;
    }

    // set encrypted one only, should be delete after remove plain fields
    if (encryptedNameExisted || encryptedIconExisted || encryptedNoticeExisted) {
        if (nameExisted || iconExisted) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "please set encrypted only";
            return false;
        }
    }

    return true;
}

bool QueryGroupInfoBody::check(GroupResponse& response)
{
    if (gid == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "gid incorrect";
        return false;
    }

    return true;
}

bool LeaveGroupBody::check(GroupResponse& response)
{
    if (gid == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "gid is not set";
        return false;
    }

    if (nextOwner.length() > kMaxGroupUidLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max next_owner length: " + std::to_string(kMaxGroupUidLength);
        return false;
    }

    return true;
}

bool InviteGroupMemberBodyV2::check(GroupResponse& response)
{
    if (members.size() > kMaxGroupCreateMemberCount) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max members count: " + std::to_string(kMaxGroupCreateMemberCount);
        return false;
    }

    if (members.size() == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "members is empty: ";
        return false;
    }

    std::set<std::string> memberSet;
    for (auto it = members.begin(); it != members.end(); ++it) {
        if (it->length() == 0 || it->length() > kMaxGroupUidLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "invalid member";
            return false;
        }
        if (memberSet.find(*it) == memberSet.end()) {
            memberSet.insert(*it);
        }
    }

    if (members.size() != memberKeys.size() || members.size() != memberGroupInfoSecrets.size()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "secrets count is not equal to members count";
        return false;
    }
    for (const auto& it : memberKeys) {
        if (it.length() > kMaxGroupKeyLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "max memberKey length: " + std::to_string(kMaxGroupKeyLength);
            return false;
        }
    }
    for (const auto& it : memberGroupInfoSecrets) {
        if (it.length() > kMaxGroupInfoSecretLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "the length of group info secret beyond " + std::to_string(kMaxGroupInfoSecretLength);
            return false;
        }
    }
    for (const auto& it : signatureInfos) {
        if (it.length() > kMaxPendingGroupUserSignatureLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "the length of signature beyond " + std::to_string(kMaxPendingGroupUserSignatureLength);
            return false;
        }
    }
     
    return true;
}



bool KickGroupMemberBody::check(GroupResponse& response, const std::string& uid)
{
    if (members.size() > kMaxGroupCreateMemberCount) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max members count: " + std::to_string(kMaxGroupCreateMemberCount);
        return false;
    }

    if (members.size() == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "members is empty";
        return false;
    }

    std::set<std::string> memberSet;
    for (auto it = members.begin(); it != members.end(); ++it) {
        if (it->length() == 0 || it->length() > kMaxGroupUidLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "invalid member";
            return false;
        }

        if (*it == uid) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "kick self";
            return false;
        }

        if (memberSet.find(*it) == memberSet.end()) {
            memberSet.insert(*it);
        }
    }

    return true;
}

bool QueryMemberListOrderedBody::check(std::string& error)
{
    if (count == 0 || count > kMaxMemberQueryCount) {
        error = "count should be in (0," + std::to_string(kMaxMemberQueryCount) + "]";
        return false;
    }

    if (startUid.size() > kMaxAccountUidLength) {
        error = "uid format error, uid: " + startUid;
        return false;
    }

    if (roles.size() > bcm::GroupUser_Role_Role_ARRAYSIZE) {
        error = "invalid roles";
        return false;
    }
    
    for (const auto& role : roles) {
        if (role < static_cast<int>(bcm::GroupUser_Role_Role_MIN)
                || role > static_cast<int>(bcm::GroupUser_Role_Role_MAX)) {
            error = "invalid roles";
            return false;
        }
    }

    return true;
}

bool QrCodeInfo::check(std::string& error)
{
    if (qrCodeToken.empty() || qrCodeToken.size() > kMaxShareSignatureLength) {
        error = "qrCodeToken is empty or the length of token is beyond " 
                + std::to_string(kMaxShareSignatureLength);
        return false;
    }
    return true;
}

bool GroupJoinRequest::check(std::string& error)
{
    if (qrCode.size() > kMaxShareSignatureLength) {
        error = "the length of qr code is beyond " + std::to_string(kMaxShareSignatureLength);
        return false;
    }

    if (qrCodeToken.size() > kMaxShareSignatureLength) {
        error = "the length of qr code token is beyond " + std::to_string(kMaxShareSignatureLength);
        return false;
    }

    if (signature.size() > kMaxPendingGroupUserSignatureLength) {
        error = "the length of signature is beyond " + std::to_string(kMaxPendingGroupUserSignatureLength);
        return false;
    }

    if (comment.size() > kMaxPendingGroupUserCommentLength) {
        error = "the length of comment is beyond " + std::to_string(kMaxPendingGroupUserCommentLength);
        return false;
    }
    return true;
}

bool QueryGroupPendingListRequest::check(std::string& error)
{
    if (startUid.size() > kMaxAccountUidLength) {
        error = "invalid uid";
        return false;
    }
    return true;
}

bool ReviewJoinResultList::check(std::string& error)
{
    for (const auto& item : list) {
        if (item.uid.size() > kMaxAccountUidLength) {
            error = "invalid uid: " + item.uid;
            return false;
        }

        if (item.groupSecret.size() > kMaxGroupKeyLength) {
            error = "the length of groupSecret beyond " + std::to_string(kMaxGroupKeyLength);
            return false;
        }

        if (item.groupInfoSecret.size() > kMaxGroupInfoSecretLength) {
            error = "the length of groupSecret beyond " + std::to_string(kMaxGroupInfoSecretLength);
            return false;
        }

        if (item.inviter.size() > kMaxAccountUidLength) {
            error = "invalid inviter uid: " + item.inviter;
            return false;
        }
    }
    return true;
}

bool CreateGroupBodyV3::check(GroupResponse& response) {
    if (name.length() > kMaxGroupNameLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max name length: " + std::to_string(kMaxGroupNameLength);
        return false;
    }

    if (icon.length() > kMaxGroupIconLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max icon length: " + std::to_string(kMaxGroupIconLength);
        return false;
    }

    if (intro.length() > kMaxGroupIntroLength) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max intro length: " + std::to_string(kMaxGroupIntroLength);
        return false;
    }

    if (broadcast != static_cast<int32_t>(Group::BROADCAST_OFF)) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "broadcast should be off";
        return false;
    }

    if (members.size() > kMaxGroupCreateMemberCount) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "max members count: " + std::to_string(kMaxGroupCreateMemberCount);
        return false;
    }

    if (members.size() == 0) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "members is empty";
        return false;
    }

    std::set<std::string> memberSet;
    for (auto it = members.begin(); it != members.end(); ++it) {
        if (it->length() == 0 || it->length() > kMaxGroupUidLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "invalid member";
            return false;
        }
        if (memberSet.find(*it) == memberSet.end()) {
            memberSet.insert(*it);
        }
    }

    if (ownerSecretKey.empty() || ownerProof.empty()) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "owner_group_info_secret and owner_proof can not be empty";
        return false;
    }

    if (members.size() != memberProofs.size() || members.size() != membersGroupInfoSecrets.size() ) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "secrets or proofs count is not equal to members count";
        return false;
    }

    for (const auto& it : memberProofs) {
        if (it.length() > kMaxGroupProofLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "max memberProof length: " + std::to_string(kMaxGroupProofLength);
            return false;
        }
    }

    for (const auto& it : membersGroupInfoSecrets) {
        if (it.length() > kMaxGroupInfoSecretLength) {
            response.code = group::ERRORCODE_PARAM_INCORRECT;
            response.msg = "the length of group info secret beyond " + std::to_string(kMaxGroupInfoSecretLength);
            return false;
        }
    }

    // group keys check
    if (groupKeysMode != GroupKeys::ONE_FOR_EACH) {
        response.code = group::ERRORCODE_PARAM_INCORRECT;
        response.msg = "group keys mode only support 0 currently!";
        return false;
    }

    return true;
}

bool DhKeysRequest::check()
{
    if (uids.size() > kMaxGroupCreateMemberCount) {
        LOGE << "invalid uid count: " << uids.size();
        return false;
    }
    for (const auto& uid : uids) {
        if (uid.size() > kMaxAccountUidLength) {
            LOGE << "invalid uid: " << uid.substr(0, kMaxAccountUidLength) << "...";
            return false;
        }
    }
    return true;
}

bool GroupJoinRequestV3::check()
{
    if (qrCode.size() > kMaxShareSignatureLength) {
        LOGE << "the length of qr code is beyond " << kMaxShareSignatureLength;
        return false;
    }

    if (qrCodeToken.size() > kMaxShareSignatureLength) {
        LOGE << "the length of qr code token is beyond " << kMaxShareSignatureLength;
        return false;
    }

    if (signature.size() > kMaxPendingGroupUserSignatureLength) {
        LOGE << "the length of signature is beyond " << kMaxPendingGroupUserSignatureLength;
        return false;
    }

    if (comment.size() > kMaxPendingGroupUserCommentLength) {
        LOGE << "the length of comment is beyond " << kMaxPendingGroupUserCommentLength;
        return false;
    }
    return true;
}

bool AddMeRequestV3::check()
{
    if (groupInfoSecret.size() > kMaxGroupInfoSecretLength) {
        LOGE << "the length of groupInfoSecret is beyond " << kMaxGroupInfoSecretLength;
        return false;
    }

    if (proof.size() > kMaxShareSignatureLength) {
        LOGE << "the length of proof is beyond " << kMaxShareSignatureLength;
        return false;
    }

    return true;
}

bool InviteGroupMemberRequestV3::check()
{
    if (members.empty() || members.size() > kMaxGroupCreateMemberCount) {
        LOGE << "invalid member count" << members.size();
        return false;
    }

    std::set<std::string> distinctUsers;
    for (const auto& m : members) {
        if (m.empty() || m.size() > kMaxGroupUidLength) {
            LOGE << "invalid uid, size: " << m.size();
            return false;
        }
        distinctUsers.emplace(m);
    }

    if (distinctUsers.size() != members.size()) {
        LOGE << "there are duplicated users in members: " << bcm::toString(members);
        return false;
    }

    // if (members.size() != memberProofs.size() || members.size() != memberGroupInfoSecrets.size()) {
    //     LOGE << "members, memberProofs, memberGroupInfoSecrets size mismatch, size: "
    //          << members.size() << ", " << memberProofs.size() << ", " << memberGroupInfoSecrets.size();
    //     return false;
    // }

    for (const auto& it : memberProofs) {
        if (it.size() > kMaxShareSignatureLength) {
            LOGE << "invalid proof length, size: " << it.size();
            return false;
        }
    }

    for (const auto& it : memberGroupInfoSecrets) {
        if (it.size() > kMaxGroupInfoSecretLength) {
            LOGE << "invalid group info secret length, size: " << it.size();
            return false;
        }
    }

    for (const auto& it : signatureInfos) {
        if (it.size() > kMaxPendingGroupUserSignatureLength) {
            LOGE << "invalid signature length, size: " << it.size();
            return false;
        }
    }
     
    return true;
}

bool ReviewJoinResultRequestV3::check()
{
    for (const auto& item : list) {
        if (item.uid.size() > kMaxAccountUidLength) {
            LOGE << "invalid uid length: " <<  item.uid.size();
            return false;
        }

        if (item.proof.size() > kMaxShareSignatureLength) {
            LOGE << "invalid proof length: " << item.proof.size();
            return false;
        }

        if (item.groupInfoSecret.size() > kMaxGroupInfoSecretLength) {
            LOGE << "invalid groupInfoSecret length: " << item.groupInfoSecret.size();
            return false;
        }

        if (item.inviter.size() > kMaxAccountUidLength) {
            LOGE << "invalid inviter length: " << item.inviter.size();
            return false;
        }
    }
    return true;
}

bool QueryMembersRequestV3::check()
{
    if (uids.size() > kMaxMemberQueryCount) {
        LOGE << "invalid uid count: " << uids.size();
        return false;
    }
    for (const auto& uid : uids) {
        if (uid.size() > kMaxAccountUidLength) {
            LOGE << "invalid uid: " << uid.substr(0, kMaxAccountUidLength) << "...";
            return false;
        }
    }
    return true;
}

} // namespace bcm
