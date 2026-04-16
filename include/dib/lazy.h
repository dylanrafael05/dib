#pragma once

#include "dib/functional.h"
#include <type_traits>

namespace dib
{
    // Represents a lazily initialized value //
    template<class T>
    class Lazy
    {
        using Getter = dib::functional::Function<T()>;

        T _value;
        Getter _getter;
        bool _initialized;

        template<class Self>
        auto &get(this Self &&self) 
        {
            if(!self._initialized)
            {
                self._initialized = true;
                self._value = self._getter();
            }

            return self._value;
        }

    public:
        Lazy(Getter getter) : _getter(getter), _initialized(false) {}

        const T &operator*() const { return get(); }
        T &operator*() { return get(); }
        
        template<class Self>
        decltype(auto) operator->(this Self &&self) 
        { 
            if constexpr(requires(const T &t) { t.operator->(); }) 
            { 
                return self.get(); 
            } 
            else 
            {
                return std::addressof(self.get());
            }
        }
    };

    template<class L>
    Lazy(L &&l) -> Lazy<std::invoke_result_t<L>>;

    // Function-like helper to create a lazily initialized value //
    template<class L>
    Lazy<std::invoke_result_t<L>> lazy(L &&l) { return Lazy(FORWARD(l)); }
}

#define LAZY_EVAL(...)  ::dib::Lazy([] { return __VA_ARGS__; })
#define LAZY_EVALC(...) ::dib::Lazy([&] { return __VA_ARGS__; })