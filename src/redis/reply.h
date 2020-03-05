#pragma once

#include <string>
#include <vector>

struct redisReply;

namespace bcm {
namespace redis {
// -----------------------------------------------------------------------------
// Section: Reply
// -----------------------------------------------------------------------------
class Reply {
public:
    Reply();
    explicit Reply(redisReply* reply);
    Reply(const Reply& other) = default;
    Reply(Reply&& other) = default;
    Reply& operator=(const Reply& other) = default;
    Reply& operator=(Reply&& other) = default;

    virtual ~Reply() {}

    bool isNull() const;
    int type() const;

    bool isSubscriptionReply() const;
    bool isSubscriptionMessage() const;
    bool isSubscribeMessage() const;
    bool isPsubscribeMessage() const;
    bool isUnsubscribeReply() const;

    bool isError() const;
    std::string getError() const;

    bool isStatus() const;
    bool checkStatus(const char* status) const;
    std::string getStatus() const;

    bool isString() const;
    std::string getString() const;

    bool isArray() const;
    int  length() const;
    std::vector<std::string> getStringList() const;
    Reply operator[](std::size_t i) const;

    bool isInteger() const;
    int64_t getInteger() const;

protected:
    struct redisReply* m_reply;
};

std::ostream& operator<<(std::ostream& os, const Reply& reply);

// -----------------------------------------------------------------------------
// Section: AutoRelReply
// -----------------------------------------------------------------------------
class AutoRelReply : public Reply {
public:
    explicit AutoRelReply(redisReply* reply);
    AutoRelReply() = default;
    AutoRelReply(const AutoRelReply& other) = default;
    AutoRelReply(AutoRelReply&& other) = default;
    AutoRelReply& operator=(const AutoRelReply& other) = default;
    AutoRelReply& operator=(AutoRelReply&& other) = default;

    virtual ~AutoRelReply();
};

} // namespace redis
} // namespace bcm