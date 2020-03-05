#pragma once
#include <memory>
#include <map>
#include <functional>

namespace bcm {

class Factory {
public:
    template<class T>
    static T* create()
    {
        const std::type_info& tid = typeid(T);
        auto it = kCreateFunctions.find(tid.hash_code());
        if (it == kCreateFunctions.end()) {
            return nullptr;
        }
        return static_cast<T*>((it->second)());
    }

    template<class T>
    static std::shared_ptr<T> make_shared()
    {
        const std::type_info& tid = typeid(T);
        auto it = kCreateFunctions.find(tid.hash_code());
        if (it == kCreateFunctions.end()) {
            return nullptr;
        }
        return static_cast<T*>((it->second)());
    }

    template<class T>
    static void registerCreator(const std::function<void*()>& func)
    {
        const std::type_info& tid = typeid(T);
        kCreateFunctions[tid.hash_code()] = func;
    }

private:
    static std::map<size_t, std::function<void*()>> kCreateFunctions;
};

}
