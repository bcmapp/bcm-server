#pragma once

#include <dao/client.h>
#include <http/http_router.h>
#include <utils/jsonable.h>

namespace bcm {

struct OpaqueContent {
    std::string content;
};

inline void to_json(nlohmann::json &j, const OpaqueContent &c) {
    j = nlohmann::json{{"content", c.content}};
}

inline void from_json(const nlohmann::json &j, OpaqueContent &c) {
    jsonable::toString(j, "content", c.content);
}

struct OpaqueIndex {
    std::string index;
};

inline void to_json(nlohmann::json &j, const OpaqueIndex &i) {
    j = nlohmann::json{{"index", i.index}};
}

inline void from_json(const nlohmann::json &j, OpaqueIndex &i) {
    jsonable::toString(j, "index", i.index);
}

class OpaqueDataController : public std::enable_shared_from_this<OpaqueDataController>
                           , public HttpRouter::Controller {
public:
    void addRoutes(HttpRouter &router) override;

    void setOpaqueData(HttpContext &context);
    void getOpaqueData(HttpContext &context);

private:
    std::shared_ptr<dao::OpaqueData> m_opaqueData{dao::ClientFactory::opaqueData()};
};

} // namespace bcm
