#pragma once
#include <boost/thread/shared_mutex.hpp>
#include "configuration_manager.h"
#include "limiter.h"

#ifdef UNIT_TEST
#define private public
#endif
namespace bcm {

class DistributedLimiter : public ILimiter,
                           public Observer<LimiterConfigUpdateEvent> {
 public:
  DistributedLimiter(const std::string& identity, 
                     const std::string& configKey, 
                     int64_t period, 
                     int64_t count)
      : m_defaultPeriod(period),
        m_defaultCount(count),
        m_identity(identity),
        m_configKey(configKey),
        m_rule(period, count) {}

  virtual LimitLevel acquireAccess(const std::string& id) override;

  virtual void currentState(LimitState& state) override;

  virtual LimitLevel limited(const std::string& id) override;

  virtual void update(const LimiterConfigUpdateEvent& event) override;

  virtual std::string identity() override { return m_identity; }

  virtual std::string configurationKey() { return m_configKey; }

 private:
  void incr(const std::string& uid, 
            const dao::LimitRule& rule, 
            int64_t& count,
            int64_t timeSlot);

 private:
  struct Item {
    Item() : timeSlot(0), count(0), mutex(new boost::shared_mutex()) {}
    int64_t timeSlot;
    int64_t count;
    std::shared_ptr<boost::shared_mutex> mutex;
  };
  int64_t m_defaultPeriod;
  int64_t m_defaultCount;
  std::string m_identity;
  std::string m_configKey;
  dao::LimitRule m_rule;
  std::map<std::string, Item> m_status;
  boost::shared_mutex m_statusMutex;
  boost::shared_mutex m_ruleMutex;
};

#ifdef UNIT_TEST
#undef private
#endif

};  // namespace bcm