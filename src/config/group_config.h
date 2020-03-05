#pragma once

#include <string>
#include <vector>
#include <utils/jsonable.h>

namespace bcm {

// -----------------------------------------------------------------------------
// Section: GroupConfigTestExceptionInject
// -----------------------------------------------------------------------------
#ifdef GROUP_EXCEPTION_INJECT_TEST

//NOTE: GROUP_EXCEPTION_INJECT_TEST is used for failure test, DONT USE IN PRODUCTION!!!
struct GroupConfigExceptionInject {

    // key update request
    bool disableGroupKeysUpdateRequest;
    int32_t delayGroupKeysUpdateRequestInMills;
    bool randomGroupKeysUpdateRequestException;
    int32_t randomGroupKeysUpdateRequestSuccessPercent;
    int32_t randomGroupKeysUpdateRequestDelayPercent;
    int32_t randomGroupKeysUpdateRequestDisablePercent;
    int32_t randomGroupKeysUpdateRequestDelayMinInMills;
    int32_t randomGroupKeysUpdateRequestDelayMaxInMills;

    // switch key
    bool disableGroupSwitchKeys;
    int32_t delayGroupSwitchKeysInMills;
    bool randomGroupSwitchKeysException;
    int32_t randomGroupSwitchKeysSuccessPercent;
    int32_t randomGroupSwitchKeysDelayPercent;
    int32_t randomGroupSwitchKeysDisablePercent;
    int32_t randomGroupSwitchKeysDelayMinInMills;
    int32_t randomGroupSwitchKeysDelayMaxInMills;

    // upload group key
    bool uploadGroupKeysException;
    int32_t uploadGroupKeysExceptionCode;
    bool randomUploadGroupKeysException;
    int32_t randomUploadGroupKeysSuccessPercent;
    int32_t randomUploadGroupKeysDelayPercent;
    int32_t randomUploadGroupKeysExceptionPercent;
    int32_t randomUploadGroupKeysDelayMinInMills;
    int32_t randomUploadGroupKeysDelayMaxInMills;
};

inline void to_json(nlohmann::json& j, const GroupConfigExceptionInject& e)
{
    j = nlohmann::json{{"disableGroupKeysUpdateRequest", e.disableGroupKeysUpdateRequest},
                       {"delayGroupKeysUpdateRequestInMills", e.delayGroupKeysUpdateRequestInMills},
                       {"randomGroupKeysUpdateRequestException", e.randomGroupKeysUpdateRequestException},
                       {"randomGroupKeysUpdateRequestSuccessPercent", e.randomGroupKeysUpdateRequestSuccessPercent},
                       {"randomGroupKeysUpdateRequestDelayPercent", e.randomGroupKeysUpdateRequestDelayPercent},
                       {"randomGroupKeysUpdateRequestDisablePercent", e.randomGroupKeysUpdateRequestDisablePercent},
                       {"randomGroupKeysUpdateRequestDelayMinInMills", e.randomGroupKeysUpdateRequestDelayMinInMills},
                       {"randomGroupKeysUpdateRequestDelayMaxInMills", e.randomGroupKeysUpdateRequestDelayMaxInMills},

                       {"disableGroupSwitchKeys", e.disableGroupSwitchKeys},
                       {"delayGroupSwitchKeysInMills", e.delayGroupSwitchKeysInMills},
                       {"randomGroupSwitchKeysException", e.randomGroupSwitchKeysException},
                       {"randomGroupSwitchKeysSuccessPercent", e.randomGroupSwitchKeysSuccessPercent},
                       {"randomGroupSwitchKeysDelayPercent", e.randomGroupSwitchKeysDelayPercent},
                       {"randomGroupSwitchKeysDisablePercent", e.randomGroupSwitchKeysDisablePercent},
                       {"randomGroupSwitchKeysDelayMinInMills", e.randomGroupSwitchKeysDelayMinInMills},
                       {"randomGroupSwitchKeysDelayMaxInMills", e.randomGroupSwitchKeysDelayMaxInMills},

                       {"uploadGroupKeysException", e.uploadGroupKeysException},
                       {"uploadGroupKeysExceptionCode", e.uploadGroupKeysExceptionCode},
                       {"randomUploadGroupKeysException", e.randomUploadGroupKeysException},
                       {"randomUploadGroupKeysSuccessPercent", e.randomUploadGroupKeysSuccessPercent},
                       {"randomUploadGroupKeysDelayPercent", e.randomUploadGroupKeysDelayPercent},
                       {"randomUploadGroupKeysExceptionPercent", e.randomUploadGroupKeysExceptionPercent},
                       {"randomUploadGroupKeysDelayMinInMills", e.randomUploadGroupKeysDelayMinInMills},
                       {"randomUploadGroupKeysDelayMaxInMills", e.randomUploadGroupKeysDelayMaxInMills}};
}

inline void from_json(const nlohmann::json& j, GroupConfigExceptionInject& e)
{

    jsonable::toBoolean(j, "disableGroupKeysUpdateRequest", e.disableGroupKeysUpdateRequest);
    jsonable::toNumber(j, "delayGroupKeysUpdateRequestInMills", e.delayGroupKeysUpdateRequestInMills);
    jsonable::toBoolean(j, "randomGroupKeysUpdateRequestException", e.randomGroupKeysUpdateRequestException);
    jsonable::toNumber(j, "randomGroupKeysUpdateRequestSuccessPercent", e.randomGroupKeysUpdateRequestSuccessPercent);
    jsonable::toNumber(j, "randomGroupKeysUpdateRequestDelayPercent", e.randomGroupKeysUpdateRequestDelayPercent);
    jsonable::toNumber(j, "randomGroupKeysUpdateRequestDisablePercent", e.randomGroupKeysUpdateRequestDisablePercent);
    jsonable::toNumber(j, "randomGroupKeysUpdateRequestDelayMinInMills", e.randomGroupKeysUpdateRequestDelayMinInMills);
    jsonable::toNumber(j, "randomGroupKeysUpdateRequestDelayMaxInMills", e.randomGroupKeysUpdateRequestDelayMaxInMills);

    jsonable::toBoolean(j, "disableGroupSwitchKeys", e.disableGroupSwitchKeys);
    jsonable::toNumber(j, "delayGroupSwitchKeysInMills", e.delayGroupSwitchKeysInMills);
    jsonable::toBoolean(j, "randomGroupSwitchKeysException", e.randomGroupSwitchKeysException);
    jsonable::toNumber(j, "randomGroupSwitchKeysSuccessPercent", e.randomGroupSwitchKeysSuccessPercent);
    jsonable::toNumber(j, "randomGroupSwitchKeysDelayPercent", e.randomGroupSwitchKeysDelayPercent);
    jsonable::toNumber(j, "randomGroupSwitchKeysDisablePercent", e.randomGroupSwitchKeysDisablePercent);
    jsonable::toNumber(j, "randomGroupSwitchKeysDelayMinInMills", e.randomGroupSwitchKeysDelayMinInMills);
    jsonable::toNumber(j, "randomGroupSwitchKeysDelayMaxInMills", e.randomGroupSwitchKeysDelayMaxInMills);

    jsonable::toBoolean(j, "uploadGroupKeysException", e.uploadGroupKeysException);
    jsonable::toNumber(j, "uploadGroupKeysExceptionCode", e.uploadGroupKeysExceptionCode);
    jsonable::toBoolean(j, "randomUploadGroupKeysException", e.randomUploadGroupKeysException);
    jsonable::toNumber(j, "randomUploadGroupKeysSuccessPercent", e.randomUploadGroupKeysSuccessPercent);
    jsonable::toNumber(j, "randomUploadGroupKeysDelayPercent", e.randomUploadGroupKeysDelayPercent);
    jsonable::toNumber(j, "randomUploadGroupKeysExceptionPercent", e.randomUploadGroupKeysExceptionPercent);
    jsonable::toNumber(j, "randomUploadGroupKeysDelayMinInMills", e.randomUploadGroupKeysDelayMinInMills);
    jsonable::toNumber(j, "randomUploadGroupKeysDelayMaxInMills", e.randomUploadGroupKeysDelayMaxInMills);
}
#endif

// -----------------------------------------------------------------------------
// Section: GroupConfig
// -----------------------------------------------------------------------------
struct GroupConfig {
    uint32_t powerGroupMin;
    uint32_t powerGroupMax;
    // when reach the normalGroupRefreshKeysMax, will not change normal group keys every time.
    uint32_t normalGroupRefreshKeysMax;
#ifdef GROUP_EXCEPTION_INJECT_TEST
    GroupConfigExceptionInject groupConfigExceptionInject;
#endif
    uint32_t keySwitchCandidateCount = 5;
};

inline void to_json(nlohmann::json& j, const GroupConfig& e)
{
    j = nlohmann::json{{"powerGroupMin", e.powerGroupMin},
                       {"powerGroupMax", e.powerGroupMax},
                       {"normalGroupRefreshKeysMax", e.normalGroupRefreshKeysMax},
                       {"keySwitchCandidateCount", e.keySwitchCandidateCount},
#ifdef GROUP_EXCEPTION_INJECT_TEST
                       {"groupConfigExceptionInject", e.groupConfigExceptionInject}
#endif
    };
}

inline void from_json(const nlohmann::json& j, GroupConfig& e)
{
    jsonable::toNumber(j, "powerGroupMin", e.powerGroupMin);
    jsonable::toNumber(j, "powerGroupMax", e.powerGroupMax);
    jsonable::toNumber(j, "normalGroupRefreshKeysMax", e.normalGroupRefreshKeysMax);
    jsonable::toNumber(j, "keySwitchCandidateCount", e.keySwitchCandidateCount);
#ifdef GROUP_EXCEPTION_INJECT_TEST
    jsonable::toGeneric(j, "groupConfigExceptionInject", e.groupConfigExceptionInject);
#endif
}


}