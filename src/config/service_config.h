#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct ServiceConfig {
    std::string host;
    int port{8080};
    int concurrency;
    struct {
        std::string certFile;
        std::string keyFile;
        std::string password;
    } ssl;
    bool websocket{true};
    std::vector<std::string> ips; // service's public ips, be used by registers
};

inline void to_json(nlohmann::json& j, const ServiceConfig& config)
{
    j = nlohmann::json{{"host", config.host},
                       {"port", config.port},
                       {"concurrency", config.concurrency},
                       {"ssl", {
                           {"certFile", config.ssl.certFile},
                           {"keyFile", config.ssl.keyFile},
                           {"password", config.ssl.password}}},
                       {"websocket", config.websocket},
                       {"ips", config.ips}};
}

inline void from_json(const nlohmann::json& j, ServiceConfig& config)
{
    jsonable::toString(j, "host", config.host);
    jsonable::toNumber(j, "port", config.port);
    jsonable::toNumber(j, "concurrency", config.concurrency);
    jsonable::toBoolean(j, "websocket", config.websocket);
    jsonable::toGeneric(j, "ips", config.ips);

    nlohmann::json ssl;
    jsonable::toGeneric(j, "ssl", ssl);
    jsonable::toString(ssl, "certFile", config.ssl.certFile);
    jsonable::toString(ssl, "keyFile", config.ssl.keyFile);
    jsonable::toString(ssl, "password", config.ssl.password);
}

}