#pragma once

#include <string>
#include "bcm_config.h"

namespace bcm {

class BcmOptions {
public:
    void parseCmd(int argc,char* argv[]);
    BcmConfig& getConfig() { return m_config; }
    static BcmOptions* getInstance()
    {
        static BcmOptions options;
        return &options;
    }

private:
    BcmOptions() {}
    void readConfig(std::string configFile);

private:
    BcmConfig m_config;
};

} // namespace bcm
