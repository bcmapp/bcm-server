
#include <cinttypes>
#include <stdexcept>
#include <thread>

#include "../test_common.h"
#include "dao/client.h"
#include "../../src/proto/dao/stored_message.pb.h"
#include "../../src/proto/dao/group_msg.pb.h"
#include "../../src/proto/dao/group_user.pb.h"
#include "../../src/proto/dao/sys_msg.pb.h"
#include "../../src/proto/dao/account.pb.h"
#include "../../src/proto/brpc/rpc_utilities.pb.h"
#include "dao/rpc_impl/group_keys_rpc_impl.h"

#include "../../src/proto/dao/device.pb.h"
#include "../../src/store/accounts_manager.h"



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



////////////////////////////////////////////////////////////////
///////////    account, messages                  //////////////
static void __packAccountDevice(std::shared_ptr<bcm::ModifyAccountDevice> newDev)
{
    newDev->set_name("11111111");
    newDev->set_authtoken("11111111");
    newDev->set_salt("11111111");
    newDev->set_signalingkey("11111111");
    
    newDev->set_gcmid("1111111");
    newDev->set_umengid("111111");
    newDev->set_apnid("111111");
    newDev->set_apntype("11111");
    newDev->set_voipapnid("111111");
    
    newDev->set_fetchesmessages(true);
    newDev->set_registrationid(0);
    newDev->set_version(1111111);
    
    bcm::SignedPreKey  tmS;
    tmS.set_keyid(22);
    tmS.set_publickey("11111111");
    tmS.set_signature("1111111");
    newDev->mutable_signedprekey(tmS);
    
    newDev->set_lastseentime(11111111);
    newDev->set_createtime(111111111);
    
    newDev->set_supportvideo(false);
    newDev->set_supportvoice(true);
    newDev->set_useragent("1111111111");
    
    bcm::ClientVersion cv;
    cv.set_ostype(bcm::ClientVersion_OSType::ClientVersion_OSType_OSTYPE_IOS);
    cv.set_osversion("vos");
    cv.set_phonemodel("ios");
    cv.set_bcmversion("bcmv23");
    cv.set_bcmbuildcode(44444);
    newDev->mutable_clientversion(cv);
    
    newDev->set_state(::bcm::Device::STATE_LOGOUT);
    newDev->set_features("1111111111111");
}

static bool __copyProtoField(const google::protobuf::Message* fromMsg,
                             const google::protobuf::Reflection *fromRef,
                             const google::protobuf::FieldDescriptor *fromField,
                             google::protobuf::Message* toMsg,
                             const google::protobuf::Reflection *toRef,
                             const google::protobuf::FieldDescriptor* toField);

static bool __copyProtoMessage(const google::protobuf::Message* fromMsg, google::protobuf::Message* toMsg)
{
    const google::protobuf::Descriptor *fromDesc = fromMsg->GetDescriptor();
    if(nullptr == fromDesc) {
        std::cout << "google::protobuf::Descriptor->GetDescriptor fromMsg is null" << std::endl;
        return false;
    }
    
    const google::protobuf::Reflection *fromRef = fromMsg->GetReflection();
    if(nullptr == fromRef) {
        std::cout << "google::protobuf::Reflection fromMsg is null " << std::endl;
        return false;
    }
    
    const google::protobuf::Descriptor *toDesc = toMsg->GetDescriptor();
    if(nullptr == toDesc) {
        std::cout << "google::protobuf::Descriptor->GetDescriptor toMsg is null" << std::endl;
        return false;
    }
    
    const google::protobuf::Reflection *toRef = toMsg->GetReflection();
    if(nullptr == toRef) {
        std::cout << "google::protobuf::Reflection toMsg is null" << std::endl;
        return false;
    }
    
    size_t count = fromDesc->field_count();
    for (size_t i = 0; i != count ; ++i)
    {
        const google::protobuf::FieldDescriptor *fromField = fromDesc->field(i);
        if(nullptr == fromField) {
            std::cout << "google::protobuf::FieldDescriptor field is null,i = " << i;
            return false;
        }
    
        const google::protobuf::FieldDescriptor* toField = toDesc->FindFieldByName(fromField->name());
        if(nullptr == toField) {
            std::cout << "google::protobuf::FieldDescriptor toField field is null, name: " << fromField->name() << std::endl;
            return false;
        }
        
        if (!__copyProtoField(fromMsg, fromRef, fromField, toMsg, toRef, toField)) {
            return false;
        }
        
    } // end for (size_t i = 0; i != count ; ++i)
    return true;
}

static bool __copyProtoField(const google::protobuf::Message* fromMsg,
                             const google::protobuf::Reflection *fromRef,
                             const google::protobuf::FieldDescriptor *fromField,
                             google::protobuf::Message* toMsg,
                             const google::protobuf::Reflection *toRef,
                             const google::protobuf::FieldDescriptor* toField)
{
    if(fromField->is_repeated()) {
        // todo
        std::cout << "field: " << fromField->name() << " ,Repeated format is not supported";
        return false;
    } else {
        if (fromRef->HasField(*fromMsg, fromField)) {
            switch (fromField->cpp_type()) {
                case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE: {
                    double value1;
                    value1 = fromRef->GetDouble(*fromMsg, fromField);
                    toRef->SetDouble(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT: {
                    float value1;
                    value1 = fromRef->GetFloat(*fromMsg, fromField);
                    toRef->SetFloat(toMsg, toField, value1);
                    break;
                    
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
                    int64_t value1;
                    value1 = fromRef->GetInt64(*fromMsg, fromField);
                    toRef->SetInt64(toMsg, toField, value1);
                    break;
                    
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
                    uint64_t value1;
                    value1 = fromRef->GetUInt64(*fromMsg, fromField);
                    toRef->SetUInt64(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
                    int32_t value1;
                    value1 = fromRef->GetInt32(*fromMsg, fromField);
                    toRef->SetInt32(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
                    uint32_t value1;
                    value1 = fromRef->GetUInt32(*fromMsg, fromField);
                    toRef->SetUInt32(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
                    bool value1;
                    value1 = fromRef->GetBool(*fromMsg, fromField);
                    toRef->SetBool(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    std::string value1;
                    value1 = fromRef->GetString(*fromMsg, fromField);
                    toRef->SetString(toMsg, toField, value1);
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    google::protobuf::Message *toM = toRef->MutableMessage(toMsg, toField);
                    const google::protobuf::Message *fromM;
                    fromM = &(fromRef->GetMessage(*fromMsg, fromField));
                    
                    if (!__copyProtoMessage(fromM, toM)) {
                        return false;
                    }
                    
                    break;//FIXME : parse Message
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_ENUM: {
                    const google::protobuf::EnumValueDescriptor *value1;
                    value1 = fromRef->GetEnum(*fromMsg, fromField);
                    toRef->SetEnum(toMsg, toField, value1);
                    break;
                }
                default:
                    break;
            }
            
        }
    } // end if(field->is_repeated())
    return true;
}

static bool __setAccountField(const std::string& fieldName,
                              google::protobuf::Message* fromMsg,
                              google::protobuf::Message* toMsg)
{
    const google::protobuf::Descriptor *fromDesc = fromMsg->GetDescriptor();
    if(nullptr == fromDesc) {
        std::cout << "google::protobuf::Descriptor->GetDescriptor fromMsg is null" << std::endl;
        return false;
    }
    
    const google::protobuf::Reflection *fromRef = fromMsg->GetReflection();
    if(nullptr == fromRef) {
        std::cout << "google::protobuf::Reflection fromMsg is null " << std::endl;
        return false;
    }
    
    const google::protobuf::Descriptor *toDesc = toMsg->GetDescriptor();
    if(nullptr == toDesc) {
        std::cout << "google::protobuf::Descriptor->GetDescriptor toMsg is null" << std::endl;
        return false;
    }
    
    const google::protobuf::Reflection *toRef = toMsg->GetReflection();
    if(nullptr == toRef) {
        std::cout << "google::protobuf::Reflection toMsg is null" << std::endl;
        return false;
    }
    
    const google::protobuf::FieldDescriptor *fromField = fromDesc->FindFieldByName(fieldName);
    if(nullptr == fromField) {
        std::cout << "google::protobuf::FieldDescriptor fromField field is null, name: " << fieldName << std::endl;
        return false;
    }

    const google::protobuf::FieldDescriptor* toField = toDesc->FindFieldByName(fieldName);
    if(nullptr == toField) {
        std::cout << "google::protobuf::FieldDescriptor toField field is null, name: " << fieldName << std::endl;
        return false;
    }
    
    if (!__copyProtoField(fromMsg, fromRef, fromField, toMsg, toRef, toField)) {
        return false;
    }
    
    return true;
}

TEST_CASE("RpcAccountManagerTest")
{
    initialize();

    std::shared_ptr<bcm::AccountsManager> accountsManager = std::make_shared<bcm::AccountsManager>();
    sleep(1);

    bcm::Account acc;
    acc.set_uid("66666666");
    acc.set_openid("222");
    acc.set_publickey("2222");
    acc.set_identitykey("22222");
    acc.set_phonenumber("222222");

    bcm::ModifyAccount  mdAccount(&acc);

    mdAccount.set_identitykey("22222222");
    mdAccount.set_name("22222222");
    mdAccount.set_avater("222222222");
    mdAccount.set_state(bcm::Account_State_NORMAL);
    mdAccount.set_nickname("22222222222");
    mdAccount.set_ldavatar("222222222222");
    mdAccount.set_hdavatar("2222222222222");
    mdAccount.set_profilekeys("22222222222222");

    ::bcm::Account_Privacy   accPrivacy;
    accPrivacy.set_acceptstrangermsg(true);
    mdAccount.mutable_privacy(accPrivacy);

    std::shared_ptr<bcm::ModifyContactsFilters> filters = mdAccount.getMutableContactsFilters();
    filters->set_algo(10);
    filters->set_content("ddddddffsfa");
    filters->set_version("dasfsa");

    std::shared_ptr<bcm::ModifyAccountDevice> mDev = mdAccount.createMutableDevice(1);
    __packAccountDevice(mDev);

    std::shared_ptr<bcm::ModifyAccountDevice> mDev2 = mdAccount.createMutableDevice(2);
    __packAccountDevice(mDev2);

    std::string missData = mdAccount.getMissFieldName();
    std::cout << "account: " << missData << std::endl;
    std::cout << "account data: " << acc.Utf8DebugString() << std::endl;

    REQUIRE(mdAccount.isModifyAccountFieldOK());

    bcm::AccountField  accFieldData = mdAccount.getAccountField();

    bcm::Account  newAcc;
    newAcc.set_uid(acc.uid());
    newAcc.set_openid("222");
    newAcc.set_publickey("2222");
    newAcc.set_identitykey("22222");
    newAcc.set_phonenumber("222222");

    for (const auto& ad : accFieldData.modifyfields()) {
        __setAccountField(ad, &acc, &newAcc);
    }

    for (auto& dev : *(accFieldData.mutable_devices())) {
        REQUIRE(dev.iscreate());
        for (auto& d : *(acc.mutable_devices())) {
            if (d.id() == dev.id()) {
                bcm::Device* targetDevice = newAcc.add_devices();
                targetDevice->set_id(dev.id());
                for (const auto& ad : dev.modifyfields()) {
                    __setAccountField(ad, &d, targetDevice);
                }
            }
        }
    }

    ::bcm::ContactsFiltersField* mf = accFieldData.mutable_modifyfilters();
    REQUIRE(!mf->isclear());
    if (mf->isclear()) {
        newAcc.clear_contactsfilters();
    }

    for (const auto& mdf : mf->modifyfields()) {
        __setAccountField(mdf, acc.mutable_contactsfilters(), newAcc.mutable_contactsfilters());
    }

    std::cout << "new account data: " << newAcc.Utf8DebugString() << std::endl;

    REQUIRE(acc.Utf8DebugString() == newAcc.Utf8DebugString());

    bcm::dao::ErrorCode  res;
    bcm::Account accOld;

    // get  not exist account
    res = accountsManager->get("eerewqtrewt", accOld);
    REQUIRE(bcm::dao::ERRORCODE_NO_SUCH_DATA == res);

    res = accountsManager->get(acc.uid(), accOld);

    if (bcm::dao::ERRORCODE_NO_SUCH_DATA == res) {
        REQUIRE(accountsManager->create(acc));
    } else {
        REQUIRE(accountsManager->updateAccount(mdAccount));
    }

    res = accountsManager->get(acc.uid(), accOld);
    REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

    std::cout << "new account data: " << accOld.Utf8DebugString() << std::endl;
    REQUIRE(acc.Utf8DebugString() == accOld.Utf8DebugString());

    {   // case 1: KeysController::setKeys
        bcm::ModifyAccount  mda(&newAcc);
        bcm::SignedPreKey  tmS;
        tmS.set_keyid(24);
        tmS.set_publickey("3333333");
        tmS.set_signature("33333333");
        std::shared_ptr<bcm::ModifyAccountDevice> tmpDev = mda.getMutableDevice(1);
        tmpDev->mutable_signedprekey(tmS);
        mda.set_identitykey("3333333333333");
        REQUIRE(accountsManager->updateAccount(mda));

        res = accountsManager->get(acc.uid(), accOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 1 new account data: " << accOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == accOld.Utf8DebugString());
    }

    {   // case 2: destroy
        bcm::ModifyAccount  mda(&newAcc);
        std::shared_ptr<bcm::ModifyAccountDevice> tmpDev = mda.getMutableDevice(1);
        mda.set_state(bcm::Account::DELETED);

        tmpDev->set_gcmid("");
        tmpDev->set_umengid("");
        tmpDev->set_apnid("");
        tmpDev->set_voipapnid("");
        tmpDev->set_apntype("");
        tmpDev->set_fetchesmessages(false);
        tmpDev->set_authtoken("");
        tmpDev->set_salt("");

        tmpDev->set_state(bcm::Device::STATE_LOGOUT);
        tmpDev->set_signalingkey("444444444");
        tmpDev->set_registrationid(1);
        tmpDev->set_name("444444444");
        tmpDev->set_supportvoice(true);
        tmpDev->set_supportvideo(true);
        tmpDev->set_lastseentime(44444444444);
        tmpDev->set_useragent("4444444444444");
        REQUIRE(accountsManager->updateAccount(mda));

        bcm::Account mOld;
        res = accountsManager->get(newAcc.uid(), mOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 2 new account data: " << mOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld.Utf8DebugString());
    }

    {  // case 3: clear_contactsfilters(); and set

        bcm::ModifyAccount  mda(&newAcc);
        mda.clear_contactsfilters();
        std::shared_ptr<bcm::ModifyContactsFilters> filters = mda.getMutableContactsFilters();
        filters->set_algo(12);
        filters->set_content("55555555");
        REQUIRE(accountsManager->updateAccount(mda));

        bcm::Account mOld;
        res = accountsManager->get(newAcc.uid(), mOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 3 new account data: " << mOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld.Utf8DebugString());
    }

    {   // case 4: clear_contactsfilters();
        bcm::ModifyAccount  mda(&newAcc);
        mda.clear_contactsfilters();
        REQUIRE(accountsManager->updateAccount(mda));

        bcm::Account mOld;
        res = accountsManager->get(newAcc.uid(), mOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 4 new account data: " << mOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld.Utf8DebugString());
    }

    {  // case 5: mutable_signedprekey
        bcm::ModifyAccount  mda(&newAcc);
        std::shared_ptr<bcm::ModifyAccountDevice> tmpDev = mda.getMutableDevice(1);
        bcm::SignedPreKey  tmS;
        tmS.set_keyid(28);
        tmS.set_publickey("6666666666");
        tmS.set_signature("66666666");
        tmpDev->mutable_signedprekey(tmS);

        REQUIRE(accountsManager->updateDevice(mda, 1));

        bcm::Account mOld;
        res = accountsManager->get(newAcc.uid(), mOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 5 new account data: " << mOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld.Utf8DebugString());
    }

    {   // case 6:
        bcm::ModifyAccount  mda(&newAcc);
        std::shared_ptr<bcm::ModifyAccountDevice> tmpDev = mda.createMutableDevice(1);
        tmpDev->set_gcmid("777777777");
        tmpDev->set_umengid("777777777");
        tmpDev->set_apnid("");
        tmpDev->set_voipapnid("");

        tmpDev->set_state(bcm::Device::STATE_LOGOUT);
        tmpDev->set_signalingkey("77777777");
        tmpDev->set_registrationid(1);
        tmpDev->set_name("777777");
        tmpDev->set_supportvoice(true);
        tmpDev->set_supportvideo(false);
        tmpDev->set_lastseentime(777777777);
        tmpDev->set_useragent("77777777");

        REQUIRE(accountsManager->updateAccount(mda));

        bcm::Account mOld;
        res = accountsManager->get(newAcc.uid(), mOld);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 6-1 new account data: " << mOld.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld.Utf8DebugString());

        REQUIRE(accountsManager->updateDevice(mda, 1));

        bcm::Account mOld2;
        res = accountsManager->get(newAcc.uid(), mOld2);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 6-2 new account data: " << mOld2.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld2.Utf8DebugString());
    }

    {   // case 7:
        bcm::ModifyAccount  mda(&newAcc);
        std::shared_ptr<bcm::ModifyAccountDevice> tmpDev = mda.createMutableDevice(2);
        tmpDev->set_name("88888");
        tmpDev->set_authtoken("88888888");
        tmpDev->set_salt("8888888");
        tmpDev->set_signalingkey("8888888888");

        tmpDev->set_gcmid("9999999");
        tmpDev->set_umengid("99999999");
        tmpDev->set_apnid("999999999999");
        tmpDev->set_apntype("99999999999999");
        tmpDev->set_voipapnid("999999");

        REQUIRE(accountsManager->updateDevice(mda, 2));

        bcm::Account mOld2;
        res = accountsManager->get(newAcc.uid(), mOld2);
        REQUIRE(bcm::dao::ERRORCODE_SUCCESS==res);

        std::cout << "case 7 new account data: " << mOld2.Utf8DebugString() << std::endl;
        REQUIRE(newAcc.Utf8DebugString() == mOld2.Utf8DebugString());
    }

}

