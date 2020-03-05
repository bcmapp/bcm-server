#pragma once

#include <nlohmann/json.hpp>
#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

namespace bcm {

namespace jsonable {

enum ValueFlags {
    OPTIONAL    = 1 << 0,
    REQUIRE     = 1 << 1,
    NOT_EMPTY   = 1 << 2
};

inline bool isOptional(int flags)
{
    return (flags & OPTIONAL) != 0;
}

inline bool isRequire(int flags)
{
    return (flags & REQUIRE) != 0;
}

inline bool isNotEmpty(int flags)
{
    return (flags & NOT_EMPTY) != 0;
}

template<typename T>
std::string toPrintable(T& t) {
    nlohmann::json j = t;
    return j.dump(-1, ' ', true);
}

template<class = void>
void toString(const nlohmann::json& j, const std::string& key, std::string& s, int flags = REQUIRE)
{
    if (isOptional(flags) && (j.find(key) == j.end() || j.find(key)->is_null())) {
        return;
    }

    s = j.at(key).get<std::string>();

    if (isNotEmpty(flags) && s.empty()) {
        throw nlohmann::detail::type_error::create(302, "string can't be empty");
    }
}

template<class = void>
void toBoolean(const nlohmann::json& j, const std::string& key, bool& b, int flags = REQUIRE)
{
    if (isOptional(flags) && (j.find(key) == j.end() || j.find(key)->is_null())) {
        return;
    }

    const nlohmann::json& value = j.at(key);
    if (!value.is_string()) {
        b =  value.get<bool>();
        return;
    }

    std::string tmp =  value.get<std::string>();
    boost::algorithm::to_lower(tmp);
    if (tmp == "true") {
        b = true;
        return;
    } else if (tmp == "false") {
        b = false;
        return;
    }

    throw nlohmann::detail::type_error::create(302, "type must be boolean, but is " + std::string(j.type_name()));
}

template <typename ArithmeticType,
        typename = typename std::enable_if<std::is_arithmetic<ArithmeticType>::value, ArithmeticType>::type>
void toNumber(const nlohmann::json& j, const std::string& key, ArithmeticType& a, int flags = REQUIRE)
{
    if (isOptional(flags) && (j.find(key) == j.end() || j.find(key)->is_null())) {
        return;
    }

    const nlohmann::json& value = j.at(key);
    if (!value.is_string()) {
        a = value.get<ArithmeticType>();
        return;
    }

    try {
        a = boost::lexical_cast<ArithmeticType>(value.get<std::string>());
    } catch (std::exception& e) {
        throw nlohmann::detail::type_error::create(302, "type must be number, but is " + std::string(j.type_name()));
    }

}

// for struct and containers
template <typename T>
void toGeneric(const nlohmann::json& j, const std::string& key, T& t, int flags = REQUIRE)
{
    if (isOptional(flags) && (j.find(key) == j.end() || j.find(key)->is_null())) {
        return;
    }

    t = j.at(key).get<T>();
}

enum JsonParseType {
    PARSETYPE_OBJECT = 0,
    PARSETYPE_ARRAY = 1
};

inline nlohmann::json safe_parse(const std::string& content, JsonParseType type = PARSETYPE_OBJECT)
{
    nlohmann::json j = nlohmann::json::parse(content, nullptr, false);

    if (j.is_discarded())
    {
        if (type == PARSETYPE_OBJECT) {
            j = nlohmann::json::object();
        } else {
            j = nlohmann::json::array();
        }
    }

    return j;
}

}

struct JsonInside {
    nlohmann::json j;
};

inline void to_json(nlohmann::json& j, const JsonInside& ji)
{
    j = ji.j;
}

inline void from_json(const nlohmann::json& j, JsonInside& ji)
{
    ji.j = j;
}

}

// support boost::optional
namespace nlohmann {
template <typename T>
struct adl_serializer<boost::optional<T>> {
    static void to_json(json& j, const boost::optional<T>& opt) {
        if (opt == boost::none) {
            j = nullptr;
        } else {
            j = *opt;
        }
    }

    static void from_json(const json& j, boost::optional<T>& opt) {
        if (j.is_null()) {
            opt = boost::none;
        } else {
            opt = j.get<T>();
        }
    }
};
}
