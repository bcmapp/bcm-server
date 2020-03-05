#pragma once

namespace bcm {
namespace dao {


template<class T>
class DaoImplCreator {
    typedef typename std::function<std::shared_ptr<T>()> TFunc;
public:
    static std::shared_ptr <T> create()
    {
        if (func != nullptr) {
            return func();
        }
        return nullptr;
    }

    static void registerFunc(TFunc f)
    {
        func = f;
    }

private:
    static TFunc func;
};

template <class T> typename DaoImplCreator<T>::TFunc DaoImplCreator<T>::func = nullptr;

}
}
