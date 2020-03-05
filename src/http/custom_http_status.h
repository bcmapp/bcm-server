#pragma once

namespace bcm {

// compitable with boost code style
enum class custom_http_status : unsigned {
    limiter_rejected = 460,
    upgrade_requried = 461,

    // group
    not_group_owner  = 480,
    not_group_user   = 481
};

std::string obsoleteReason(unsigned status);

}
