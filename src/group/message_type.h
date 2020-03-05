#pragma once

namespace bcm {

enum MessageType {
    GROUP_CHAT_MODE_MESSAGE = 1,
    GROUP_SUB_MODE_MESSAGE = 2,
    GROUP_INFO_UPDATE = 3,
    GROUP_MEMBER_UPDATE = 4,
    GROUP_GAME_MESSAGE = 5
};

enum InternalMessageType {
    INTERNAL_USER_ENTER_GROUP = 1,
    INTERNAL_USER_QUIT_GROUP = 2,
    INTERNAL_USER_CHANGE_ROLE = 3,
	INTERNAL_USER_MUTE_GROUP = 4,
	INTERNAL_USER_UNMUTE_GROUP = 5
};

enum UserStatusType {
    USER_OFFLINE = 0,
    USER_ONLINE = 1
};

enum GroupMemberUpdateAction {
    ENTER_GROUP = 1,
    UPDATE_INFO = 2,
    QUIT_GROUP = 3,
    CHANGE_KEY = 4
};

enum class GroupStatus {
    DEFAULT = 0,
    MUTED = 1
};

//enum class GroupBroadcast {
//    OFF = 0,
//    ON
//};

enum class GroupRole {
    UNVALID 	= -1,
    UNDEFINE 	= 0,
    OWNER 		= 1,
    ADMINISTROR = 2,
    MEMBER		= 3,
    SUBSCRIBER	= 4
};

enum class GroupEncrypt {
    UNVALID = -1,
    OFF = 0,
    ON = 1
};

enum class GroupAtType {
	UNVALID = 0,
	AT_LIST = 1,
	AT_ALL = 2
};


enum class GroupBroadcastType {
	UNVALID = -1,
	NO_BROADCAST = 0,
	INNER_BROADCAST = 1,
	INTER_BROADCAST = 2
};

}

