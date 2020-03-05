#pragma once

#include <functional>
#include <type_traits>
#include <event2/event.h>
#include <event2/event_struct.h>

class Task {
public:
    static void asyncInvoke(event_base* base, std::function<void()> func)
    {
        auto task = new Task(base, func);
        task->active();
    }

private:
    Task(event_base* base, std::function<void()> func)
        : m_func(std::move(func))
    {
        event_assign(&m_event, base, -1, 0, &Task::run, reinterpret_cast<void*>(this));

    }

    void active()
    {
        event_active(&m_event, 0, 0);
    }

    static void run(int, short, void* arg)
    {
        auto* pThis = reinterpret_cast<Task*>(arg);
        pThis->m_func();
        delete pThis;
    }

private:
    std::function<void()> m_func;
    struct event m_event{};
};