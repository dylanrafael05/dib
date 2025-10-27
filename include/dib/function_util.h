#ifndef __FUNCTION_UTIL_H
#define __FUNCTION_UTIL_H

#include <concepts>
#include <functional>
#include <exception>

#include "dib/preprocess.h"
#include "dib/meta.h"
#include "dib/debug.h"
#include "dib/types.h"
#include "dib/vectors.h"

namespace dib::functional
{
    template<class... T>
    class OneOfType
    {
    private:
        std::tuple<T...> _values;

    public:
        OneOfType(const OneOfType &) = delete;
        OneOfType(OneOfType &&) = delete;
        explicit OneOfType(auto &&...vals)
            : _values(DIB_FWD(vals)...)
        {}

        friend bool operator==(types::not_convertible_to<OneOfType> auto &&lhs, const OneOfType &rhs)
        {
            bool ret = false;

            [&]<size_t... I>(std::index_sequence<I...>){
                ([&] {
                    ret |= lhs == std::get<I>(rhs._values);
                }(), ...);
            }(std::make_index_sequence<sizeof...(T)>{});

            return ret;
        }
    };

    template<class... Args>
    OneOfType<Args...> one_of(Args &&...args)
    {
        return OneOfType<Args...>(DIB_FWD(args)...);
    }

    template<class... T>
    struct overload : T...
    {
        using T::operator()...;
    };

    template<class... T>
    overload(T...) -> overload<T...>;

    struct Absorber
    {
        template<class T>
        Absorber &operator=(T &&) {return *this;}
    };

    struct CountingIterator
    {
        size_t count;

        CountingIterator &operator++()
        {
            count++;
            return *this;
        }

        CountingIterator operator++(int)
        {
            auto copy = *this;
            count++;
            return copy;
        }

        Absorber operator*() const {return {};}
    };
    
    template<class T>
    constexpr bool always_true_v = true;
    
    namespace detail
    {
        template<class R, class... Args>
        class StackFunction_Impl
        {
            R(*function)(void *env, Args ...args);
            void *env;

        public:
            R operator()(Args ...args) const
            {
                return function(env, std::forward<Args>(args)...);
            }

            StackFunction_Impl(R(*function)(void *env, Args ...args), void *env)
                : function(function), env(env)
            {}

            template<class Lambda>
            StackFunction_Impl(const Lambda &lambda)
                : function([](void *env, Args ...args) -> R {return (*(const Lambda *)(env))(std::forward<Args>(args)...);})
                , env((void*) &lambda)
            {}
        };
    }

    template<class T>
    struct StackFunction {};

    template<class R, class... Args>
    struct StackFunction<R(Args...)> : public detail::StackFunction_Impl<R, Args...>
    {
        using detail::StackFunction_Impl<R, Args...>::StackFunction_Impl;
    };

    namespace detail
    {
        template<class R, class... Args>
        struct SmallFunctionBase
        {
            virtual R operator()(Args...) const = 0;
            virtual SmallFunctionBase *clone() const = 0;
            virtual ~SmallFunctionBase() {}
        };

        template<class T, class R, class... Args>
        struct LambdaFunction : T, public SmallFunctionBase<R, Args...>
        {
            LambdaFunction(const T &lam)
                : T(lam)
            {}

            LambdaFunction(T &&lam)
                : T(std::move(lam))
            {}

            virtual R operator()(Args ...args) const override
            {
                LambdaFunction *self = const_cast<LambdaFunction*>(this);
                return self->T::operator()(std::forward<Args>(args)...);
            }

            virtual SmallFunctionBase<R, Args...> *clone() const override
            {
                return new LambdaFunction(*(const T*)this);
            }
        };
    }

    template<class> class SmallFunction {};
    
    template<class R, class... Args> 
    class SmallFunction<R(Args...)>
    {
        union
        {
            R (*_fn_ptr)(Args...);
            detail::SmallFunctionBase<R, Args...> *_heap_ptr;
        };
        
        bool _is_fn_ptr;

        void move_from(SmallFunction &&other)
        {
            _is_fn_ptr = other._is_fn_ptr;
            if(_is_fn_ptr)
            {
                _fn_ptr = other._fn_ptr;
            }
            else 
            {
                _heap_ptr = other._heap_ptr;
            }

            other._fn_ptr = nullptr;
            other._is_fn_ptr = true;
        }

        void copy_from(const SmallFunction &other)
        {
            _is_fn_ptr = other._is_fn_ptr;

            if(other._heap_ptr == nullptr)
            {
                _fn_ptr = nullptr;
            }
            else if(_is_fn_ptr)
            {
                _fn_ptr = other._fn_ptr;
            }
            else 
            {
                _heap_ptr = other._heap_ptr->clone();
            }
        }

        void destroy()
        {
            if(_is_fn_ptr || !_heap_ptr) return;

            delete _heap_ptr;
        }

    public:
        constexpr SmallFunction() : _fn_ptr(nullptr), _is_fn_ptr(true) {}
        constexpr SmallFunction(std::nullptr_t) : _fn_ptr(nullptr), _is_fn_ptr(true) {}

        SmallFunction(R (*ptr)(Args...))
            : _fn_ptr(ptr), _is_fn_ptr(true)
        {}

        template<dib::meta::value_differs_from<SmallFunction<R(Args...)>> L>
        SmallFunction(L &&lambda)
            : _heap_ptr(new detail::LambdaFunction<L, R, Args...>(std::forward<L>(lambda)))
            , _is_fn_ptr(false)
        {}

        SmallFunction(const SmallFunction &other)
        {
            copy_from(other);
        }
        
        SmallFunction(SmallFunction &&other)
        {
            move_from(std::move(other));
        }

        ~SmallFunction()
        {
            destroy();
        }

        SmallFunction &operator=(const SmallFunction &other)
        {
            destroy();
            copy_from(other);

            return *this;
        }
        SmallFunction &operator=(SmallFunction &&other)
        {
            destroy();
            move_from(std::move(other));

            return *this;
        }

        constexpr bool is_raw_ptr() const
        {
            return _is_fn_ptr;
        }
        constexpr R (*raw_ptr() const)(Args...)
        {
            return _fn_ptr;
        }

        constexpr bool empty() const
        {
            return _fn_ptr == nullptr;
        }

        constexpr operator bool() const {return _fn_ptr != nullptr;}

        R operator()(Args... args) const
        {
            if(_is_fn_ptr) [[likely]]
            {
                return _fn_ptr(std::forward<Args>(args)...);
            }
            
            return _heap_ptr->operator()(std::forward<Args>(args)...);
        }
    };

    class BadMultifunctionCall : std::exception
    {
        const char *what() const override
        {
            return "bad multifunction call.";
        }
    };

    template<class MergeOperator, class R, class... Args>
    class BasicMultifunction
    {
        dib::structures::SvoVector<std::function<R(Args...)>> _functions;

    public:
        void clear() { _functions.clear(); }

        operator bool() const { return !_functions.empty(); }

        R operator()(Args... args) const
        {
            if constexpr (std::is_void_v<R>)
            {
                for (auto &f : _functions)
                    f(std::forward<Args>(args)...);
            }
            else
            {
                if (_functions.empty()) throw BadMultifunctionCall{};

                R result = _functions.front()(std::forward<Args>(args)...);
                for (size_t i = 1; i < _functions.size(); i++)
                    result = MergeOperator{}(result, _functions[i](std::forward<Args>(args)...));

                return result;
            }
        }

        template<std::invocable<Args...> Callable>
        void append(Callable &&c)
        {
            _functions.emplace_back(std::forward<Callable>(c));
        }
    };

    template<class... Args>
    using Multievent = BasicMultifunction<void, void, Args...>;

    template<class... Args>
    using Multipredicate = BasicMultifunction<std::logical_and<>, bool, Args...>;

    template<class T>
    struct implicit_cast_t
    {
        constexpr T operator()(auto &&x) const
        {
            return std::forward<decltype(x)>(x);
        }
    };

    template<class T>
    constexpr implicit_cast_t<T> implicit_cast = {};
}

#endif