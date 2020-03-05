#pragma once

#include <nlohmann/json.hpp>
#include <boost/any.hpp>
#include <boost/algorithm/string.hpp>
#include "log.h"

namespace bcm {

class JsonSerializer {
public:
    virtual bool serialize(const boost::any& src, std::string& des) = 0;
    virtual bool deserialize(const std::string& src, boost::any& des) = 0;
};

// for using this serializer, should provide to_json/from_json function in T's namespace
template<typename T>
class JsonSerializerImp : public JsonSerializer {
public:
    bool serialize(const boost::any& src, std::string& des) override
    {
        if (src.type() != typeid(T)) {
            return false;
        }

        const auto& attr = boost::any_cast<T>(src);
        nlohmann::json j = attr;
        des = j.dump(-1, ' ', true);

        return true;
    }

    bool deserialize(const std::string& src, boost::any& des) override
    {
        nlohmann::json j = nlohmann::json::parse(src, nullptr, false);
        if (!j.is_object()) {
            return false;
        }

        try {
            auto attr = j.get<T>();
            des = std::move(attr);
        } catch (const std::exception& e) {
            LOGD << "json deserialize " << src << " failed: " << e.what();
            return false;
        }

        return true;
    }
};

}