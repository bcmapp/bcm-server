#include "reply.h"

#include <hiredis/hiredis.h>

#include <string.h>
#include <stdexcept>

namespace bcm {
namespace redis {

static const char kSubscribeReplyTag[] = "subscribe";
static const char kPsubscribeReplyTag[] = "psubscribe";

static const char kUnsubscribeReplyTag[] = "unsubscribe";
static const char kPunsubscribeReplyTag[] = "punsubscribe";

static const char kSubscribeMessageTag[] = "message";
static const char kPsubscribeMessageTag[] = "pmessage";

// -----------------------------------------------------------------------------
// Section: Reply
// -----------------------------------------------------------------------------
Reply::Reply() : m_reply(nullptr)
{
}

Reply::Reply(redisReply* reply) : m_reply(reply)
{
}

bool Reply::isNull() const
{
    return (nullptr == m_reply);
}

int Reply::type() const
{
    return ( (m_reply != nullptr) ? m_reply->type : 0 );
}

bool Reply::isSubscriptionReply() const
{
    if (length() == 3) {
        struct redisReply* first = m_reply->element[0];
        if ((strncmp(first->str, kSubscribeReplyTag, first->len) == 0) ||
            (strncmp(first->str, kPsubscribeReplyTag, first->len) == 0)) {
            return true;
        }
    }
    return false;
}

bool Reply::isSubscriptionMessage() const
{
    return (isSubscribeMessage() || isPsubscribeMessage());
}

bool Reply::isSubscribeMessage() const
{
    if (length() == 3) {
        struct redisReply* first = m_reply->element[0];
        if (strncmp(first->str, kSubscribeMessageTag, first->len) == 0) {
            return true;
        }
    }
    return false;
}

bool Reply::isPsubscribeMessage() const
{
    if (length() == 4) {
        struct redisReply* first = m_reply->element[0];
        if (strncmp(first->str, kPsubscribeMessageTag, first->len) == 0) {
            return true;
        }
    }
    return false;
}

bool Reply::isUnsubscribeReply() const
{
    if (length() == 3) {
        struct redisReply* first = m_reply->element[0];
        if ((strncmp(first->str, kUnsubscribeReplyTag, first->len) == 0) ||
            (strncmp(first->str, kPunsubscribeReplyTag, first->len) == 0)) {
            return true;
        }
    }
    return false;
}

bool Reply::isError() const
{
    return ( (m_reply != nullptr) && (REDIS_REPLY_ERROR == m_reply->type) );
}

std::string Reply::getError() const
{
    if (isError()) {
        return std::string(m_reply->str, m_reply->len);
    }
    return "";
}

bool Reply::isStatus() const
{
    return ( (m_reply != nullptr) && (REDIS_REPLY_STATUS == m_reply->type) );
}

bool Reply::checkStatus(const char* status) const
{
    if (isStatus()) {
        return (strncmp(m_reply->str, status, m_reply->len) == 0);
    }
    return false;
}

std::string Reply::getStatus() const
{
    if (isStatus()) {
        return std::string(m_reply->str, m_reply->len);
    }
    return "";
}

bool Reply::isString() const
{
    return ( (m_reply != nullptr) && (REDIS_REPLY_STRING == m_reply->type) );
}

std::string Reply::getString() const
{
    if (isString()) {
        return std::string(m_reply->str, m_reply->len);
    }
    return "";
}

bool Reply::isArray() const
{
    return ( (m_reply != nullptr) && (REDIS_REPLY_ARRAY == m_reply->type) );
}

int Reply::length() const
{
    return (isArray() ? m_reply->elements : 0);
}

std::vector<std::string> Reply::getStringList() const
{
    std::vector<std::string> vec;
    for (std::size_t i = 0; i < m_reply->elements; i++) {
        Reply r(m_reply->element[i]);
        if (r.isString()) {
            vec.emplace_back(r.getString());
        } else if (r.isError()) {
            vec.emplace_back(r.getError());
        } else if (r.isStatus()) {
            vec.emplace_back(r.getStatus());
        } else if (r.isInteger()) {
            vec.emplace_back(std::to_string(r.getInteger()));
        }
    }
    return vec;
}

Reply Reply::operator[](std::size_t i) const
{
    if (!isArray()) {
        throw std::invalid_argument("type is not array");
    } else if (i >= m_reply->elements) {
        throw std::out_of_range("out of range");
    } else {
        return Reply(m_reply->element[i]);
    }
}

bool Reply::isInteger() const
{
    return ( (m_reply != nullptr) && (REDIS_REPLY_INTEGER == m_reply->type) );
}

int64_t Reply::getInteger() const
{
    if (isInteger()) {
        return m_reply->integer;
    }
    return 0;
}

std::ostream& operator<<(std::ostream& os, const Reply& reply)
{
    if (reply.isNull()) {
        os << std::string("NULL");
        return os;
    }
    switch (reply.type()) {
    case REDIS_REPLY_STRING:
        os << reply.getString();
        break;
    case REDIS_REPLY_ARRAY:
        os << std::string("[");
        for (int i = 0; i < reply.length(); i++) {
            if (i > 0) {
                os << std::string(", ");
            }
            os << reply[i];
        }
        os << std::string("]");
        break;
    case REDIS_REPLY_INTEGER:
        os << std::to_string(reply.getInteger());
        break;
    case REDIS_REPLY_NIL:
        os << std::string("NIL");
        break;
    case REDIS_REPLY_STATUS:
        os << reply.getStatus();
        break;
    case REDIS_REPLY_ERROR:
        os << reply.getError();
        break;
    }
    return os;
}

// -----------------------------------------------------------------------------
// Section: AutoRelReply
// -----------------------------------------------------------------------------
AutoRelReply::AutoRelReply(redisReply* reply) : Reply(reply) {}

AutoRelReply::~AutoRelReply()
{
    if (nullptr != m_reply) {
        freeReplyObject(reinterpret_cast<void*>(m_reply));
    }
}

} // namespace redis
} // namespace bcm