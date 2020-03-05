#include "api_matcher.h"
#include <boost/algorithm/string.hpp>

namespace bcm {

bool ApiMatcher::match(const Api& pattern, const Api& target) 
{
    if (pattern.method != target.method) {
        return false;
    }
    std::vector<std::string> patterns;
    std::vector<std::string> targets;
    boost::split(patterns, pattern.name, boost::is_any_of("/"));
    boost::split(targets, target.name, boost::is_any_of("/"));
    if (patterns.size() != targets.size()) {
        return false;
    }
    for (size_t i = 0; i < patterns.size(); i++) {
        if (!matchInternal(patterns[i], targets[i])) {
            return false;
        }
    }
    return true;
}

bool ApiMatcher::match(const std::string& pattern, const std::string& target) 
{
    std::vector<std::string> patterns;
    std::vector<std::string> targets;
    boost::split(patterns, pattern, boost::is_any_of("/"));
    boost::split(targets, target, boost::is_any_of("/"));
    if (patterns.size() != targets.size()) {
        return false;
    }
    for (size_t i = 0; i < patterns.size(); i++) {
        if (!matchInternal(patterns[i], targets[i])) {
            return false;
        }
    }
    return true;
}

bool ApiMatcher::matchAndFetch(const Api& pattern, 
                               const Api& target, 
                               const std::string& key,
                               std::string& value) 
{
    if (pattern.method != target.method) {
        return false;
    }
    std::vector<std::string> patterns;
    std::vector<std::string> targets;
    boost::split(patterns, pattern.name, boost::is_any_of("/"));
    boost::split(targets, target.name, boost::is_any_of("/"));
    if (patterns.size() != targets.size()) {
        return false;
    }
    size_t matchIndex = patterns.size();
    for (size_t i = 0; i < patterns.size(); i++) {
        if (!matchInternal(patterns[i], targets[i])) {
            return false;
        }
        if (patterns[i] == key) {
            matchIndex = i;
        }
    }
    if (matchIndex < patterns.size()) {
        value = targets[matchIndex];
    }
    return true;
}

bool ApiMatcher::matchAndFetch(const std::string& pattern, 
                               const std::string& target, 
                               const std::string& key,
                               std::string& value) 
{
    std::vector<std::string> patterns;
    std::vector<std::string> targets;
    boost::split(patterns, pattern, boost::is_any_of("/"));
    boost::split(targets, target, boost::is_any_of("/"));
    if (patterns.size() != targets.size()) {
        return false;
    }
    size_t matchIndex = patterns.size();
    for (size_t i = 0; i < patterns.size(); i++) {
        if (!matchInternal(patterns[i], targets[i])) {
            return false;
        }
        if (patterns[i] == key) {
            matchIndex = i;
        }
    }
    if (matchIndex < patterns.size()) {
        value = targets[matchIndex];
    }
    return true;
}

bool ApiMatcher::matchInternal(const std::string& pattern, const std::string& target)
{
    if (!pattern.empty() && pattern[0] == ':') {
        return true;
    }
    return pattern == target;
}

}
