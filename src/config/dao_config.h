#pragma once

#include <utils/jsonable.h>

namespace bcm {

struct DaoConfig {
    struct {
        std::string host;
        int port{5432};
        std::string username;
        std::string password;
        bool read{true};
        bool write{true};
    } postgres;

    struct {
        std::string config;
        std::string cluster;
        bool read{true};
        bool write{true};
    } pegasus;

    struct {
        std::string hosts;
        std::string proto;
        std::string connType;
        int retries;
        std::string balancer;
        std::string keyPath;
        std::string certPath;
    } remote;

    int timeout{3000};
    std::string clientImpl;
};

inline void to_json(nlohmann::json& j, const DaoConfig& config)
{
    j = nlohmann::json{{"postgres", {
                            {"host", config.postgres.host},
                            {"port", config.postgres.port},
                            {"username", config.postgres.username},
                            {"password", config.postgres.password},
                            {"read", config.postgres.read},
                            {"write", config.postgres.write}}},
                        {"pegasus", {
                            {"config", config.pegasus.config},
                            {"cluster", config.pegasus.cluster},
                            {"read", config.pegasus.read},
                            {"write", config.pegasus.write}}},
                       {"remote", {
                           {"hosts", config.remote.hosts},
                           {"proto", config.remote.proto},
                           {"connType", config.remote.connType},
                           {"retries", config.remote.retries},
                           {"balancer", config.remote.balancer},
                           {"keyPath", config.remote.keyPath},
                           {"certPath", config.remote.certPath}}}, 
                       {"timeout", config.timeout},
                       {"clientImpl", config.clientImpl}};
}

inline void from_json(const nlohmann::json& j, DaoConfig& config)
{
    nlohmann::json postgres;
    jsonable::toGeneric(j, "postgres", postgres);
    jsonable::toString(postgres, "host", config.postgres.host);
    jsonable::toNumber(postgres, "port", config.postgres.port);
    jsonable::toString(postgres, "username", config.postgres.username);
    jsonable::toString(postgres, "password", config.postgres.password);
    jsonable::toBoolean(postgres, "read", config.postgres.read);
    jsonable::toBoolean(postgres, "write", config.postgres.write);

    nlohmann::json pegasus;
    jsonable::toGeneric(j, "pegasus", pegasus);
    jsonable::toString(pegasus, "config", config.pegasus.config);
    jsonable::toString(pegasus, "cluster", config.pegasus.cluster);
    jsonable::toBoolean(pegasus, "read", config.pegasus.read);
    jsonable::toBoolean(pegasus, "write", config.pegasus.write);
    
    nlohmann::json remote;
    jsonable::toGeneric(j, "remote", remote);
    jsonable::toString(remote, "hosts", config.remote.hosts);
    jsonable::toString(remote, "proto", config.remote.proto);
    jsonable::toString(remote, "connType", config.remote.connType);
    jsonable::toNumber(remote, "retries", config.remote.retries);
    jsonable::toString(remote, "balancer", config.remote.balancer);
    jsonable::toString(remote, "keyPath", config.remote.keyPath);
    jsonable::toString(remote, "certPath", config.remote.certPath);

    jsonable::toNumber(j, "timeout", config.timeout);
    jsonable::toString(j, "clientImpl", config.clientImpl);
}

}