#include "limiter_config_update.h"
#include "configuration_manager.h"

namespace bcm {

void LimiterConfigUpdate::run()
{
    LimiterConfigurationManager::getInstance()->reloadConfiguration();
}

}
