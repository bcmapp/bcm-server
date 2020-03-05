#pragma once
#include <memory>

namespace bcm {

template<class EventType>
class Observer {
public:
    virtual void update(const EventType& event) = 0;
};

template<class EventType>
class Observable {
public:
    typedef Observer<EventType> ObserverType;
public:
    virtual void registerObserver(const std::shared_ptr<ObserverType>& observer) = 0;
    virtual void unregisterObserver(const std::shared_ptr<ObserverType>& observer) = 0;
    virtual void notify(const EventType& event) = 0;
};

}