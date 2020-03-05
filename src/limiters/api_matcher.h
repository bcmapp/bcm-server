#pragma once
#include "common/api.h"

namespace bcm {

class IApiMatcher {
public:
    virtual bool match(const Api& pattern, const Api& garget) = 0;
    virtual bool matchAndFetch(const Api& pattern, 
                               const Api& garget, 
                               const std::string& key,
                               std::string& value) = 0;
    virtual bool match(const std::string& pattern, const std::string& target) = 0;

    virtual bool matchAndFetch(const std::string& pattern, 
                               const std::string& target, 
                               const std::string& key,
                               std::string& value) = 0;
};

class ApiMatcher : public IApiMatcher {
public:
    virtual bool match(const Api& pattern, const Api& target) override;

    virtual bool matchAndFetch(const Api& pattern, 
                               const Api& target, 
                               const std::string& key,
                               std::string& value) override;
    
    virtual bool matchAndFetch(const std::string& pattern, 
                               const std::string& target, 
                               const std::string& key,
                               std::string& value) override;

    virtual bool match(const std::string& pattern, const std::string& target) override;
private:
    bool matchInternal(const std::string& pattern, const std::string& target);
};

}