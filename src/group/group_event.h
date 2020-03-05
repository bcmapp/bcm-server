#include <nlohmann/json.hpp>

namespace bcm {

struct GroupEvent {
    int type;
    std::string uid;
    uint64_t gid;
};

inline void from_json(const nlohmann::json& j, GroupEvent& e)
{
    j.at("type").get_to(e.type);
    j.at("uid").get_to(e.uid);
    j.at("gid").get_to(e.gid);
}

inline void to_json(nlohmann::json& j, const GroupEvent& e)
{
    j = { {"type", e.type},
          {"uid", e.uid},
          {"gid", e.gid} };
}

} // namespace bcm