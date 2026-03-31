#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>

#include "dib/preprocess.h"
#include "dib/debug.h"
#include "dib/types.h"
#include "dib/vector.h"
#include "dib/fn.h"

namespace dib::functional
{
    /// A helper type which can be compared against, such that
    /// x == one_of(y, z) is the same as x == y || x == z
    template<class... T>
    class OneOfComparison
    {
    private:
        std::tuple<T...> _values;

    public:
        OneOfComparison(const OneOfComparison &) = delete;
        OneOfComparison(OneOfComparison &&) = delete;
        explicit OneOfComparison(auto &&...vals)
            : _values(FORWARD(vals)...)
        {}

        friend bool operator==(types::NotConvertibleTo<OneOfComparison> auto &&lhs, const OneOfComparison &rhs)
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

    /// A helper function to create a comparison such that
    /// x == one_of(y, z) is the same as x == y || x == z
    template<class... Args>
    OneOfComparison<Args...> one_of(Args &&...args)
    {
        return OneOfComparison<Args...>(FORWARD(args)...);
    }

    /// A helper type which merges many lambdas into one,
    /// overloaded lambda.
    template<class... T>
    struct Overload : T...
    {
        using T::operator()...;
    };

    template<class... T>
    Overload(T...) -> Overload<T...>;
    
    /// A helper variable which is always 'true'
    template<class T>
    constexpr bool always_true = true;

    template<class F>
    constexpr fn<F> to_fn_ptr(std::convertible_to<fn<F>> auto &&lmb)
    {
        return (fn<F>)lmb;
    }
    
    /// Implementation of the 'FunctionRef' class
    /// is partially completed below.
    namespace detail
    {
        template<class R, class... Args>
        class FunctionRef_Impl
        {
            R(*function)(void *env, Args ...args);
            void *env;

        public:
            constexpr R operator()(Args ...args) const
            {
                return function(env, FORWARD(args)...);
            }

            constexpr operator bool() const { return function; }

            constexpr FunctionRef_Impl() : function(nullptr), env(nullptr) {}
            constexpr FunctionRef_Impl(std::nullptr_t) : FunctionRef_Impl() {}
            
            template<class T, std::invocable<T *, Args...> Fn> requires (types::IsSameAs<std::invoke_result_t<Fn &&, T *, Args...>, R>)
            constexpr FunctionRef_Impl(Fn &&function, T *env)
                : function((R(*)(void *, Args...))(void*)to_fn_ptr<R(T *, Args...)>(function))
                , env(env)
            {}

            template<class Lambda>
            constexpr FunctionRef_Impl(const Lambda &lambda)
                : function([](void *env, Args ...args) -> R {
                    return (*(const Lambda *)(env))(FORWARD(args)...); })
                , env((void*) &lambda)
            {}
        };
    }

    /// A function-like type which refers to a function or lambda for as long
    /// as it may live. Using a FunctionRef after the associated function dies
    /// is undefined behavior.
    template<class T>
    struct FunctionRef {};

    template<class R, class... Args>
    struct FunctionRef<R(Args...)> : public detail::FunctionRef_Impl<R, Args...>
    {
        using detail::FunctionRef_Impl<R, Args...>::FunctionRef_Impl;
    };

    /// Implementation of the 'Function' class
    /// is partially completed below.
    namespace detail
    {
        /// Polymorphic base class which provides an interface
        /// for calling an underlying lambda
        template<class R, class... Args>
        struct FunctionImpl
        {
            virtual R operator()(Args...) const = 0;
            virtual FunctionImpl *clone() const = 0;
            virtual ~FunctionImpl() {}
        };

        /// Template implementation of FunctionImpl for lambdas
        template<class T, class R, class... Args>
        struct FunctionImpl_Lambda : T, public FunctionImpl<R, Args...>
        {
            FunctionImpl_Lambda(const T &lam)
                : T(lam)
            {}

            FunctionImpl_Lambda(T &&lam)
                : T(MOVE(lam))
            {}

            virtual R operator()(Args ...args) const override
            {
                return types::remove_const(this)->T::operator()(FORWARD(args)...);
            }

            virtual FunctionImpl<R, Args...> *clone() const override
            {
                return new FunctionImpl_Lambda(*(const T*)this);
            }
        };
    }

    /// A function-like type (like std::function) which does not allocate if
    /// initialized via function pointer.
    template<class> class Function {};
    
    template<class R, class... Args> 
    class Function<R(Args...)>
    {
        // Store both the function pointer and heap-bound function impl
        union
        {
            FunctionRef<R(Args...)> _fn_ptr;
            detail::FunctionImpl<R, Args...> *_heap_ptr;
        };
        
        // Whether or not this is a function pointer or heap pointer
        bool _is_fn_ptr;

        // Class operations
        void move_from(Function &&other)
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

        void copy_from(const Function &other)
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
        constexpr Function() : _fn_ptr(nullptr), _is_fn_ptr(true) {}
        constexpr Function(std::nullptr_t) : _fn_ptr(nullptr), _is_fn_ptr(true) {}

        Function(R (*ptr)(Args...))
            : _fn_ptr(ptr), _is_fn_ptr(true)
        {}
        
        Function(const FunctionRef<R(Args...)> &ptr)
            : _fn_ptr(ptr), _is_fn_ptr(true)
        {}

        template<dib::types::NotValueSame<Function<R(Args...)>> L>
        Function(L &&lambda)
            : _heap_ptr(new detail::FunctionImpl_Lambda<std::remove_reference_t<L>, R, Args...>(FORWARD(lambda)))
            , _is_fn_ptr(false)
        {}

        Function(const Function &other)
        {
            copy_from(other);
        }
        
        Function(Function &&other)
        {
            move_from(MOVE(other));
        }

        ~Function()
        {
            destroy();
        }

        Function &operator=(const Function &other)
        {
            destroy();
            copy_from(other);

            return *this;
        }
        Function &operator=(Function &&other)
        {
            destroy();
            move_from(MOVE(other));

            return *this;
        }

        constexpr bool is_raw_ptr() const
        {
            return _is_fn_ptr;
        }
        constexpr auto raw_ptr() const
        {
            ASSERT(is_raw_ptr());
            return _fn_ptr;
        }

        /// Check if this instance is considered empty; i.e. it has been
        /// moved from or contains a null pointer.
        constexpr bool empty() const
        {
            return !_fn_ptr;
        }

        /// Check that this instance is nonempty
        constexpr operator bool() const { return _fn_ptr; }

        /// Call the function represented by this instance.
        /// Throws if empty.
        R operator()(Args... args) const
        {
            ASSERT(!empty());

            if(_is_fn_ptr) [[likely]]
            {
                return _fn_ptr(FORWARD(args)...);
            }
            
            return _heap_ptr->operator()(FORWARD(args)...);
        }
    };

    template<class R, class ...Args>
    Function(R(*)(Args...)) -> Function<R(Args...)>;
    template<class R, class ...Args>
    Function(const FunctionRef<R(Args...)> &) -> Function<R(Args...)>;

    template<class MergeOperator, class R, class... Args>
    class BasicMultifunction
    {
        dib::structures::Vector<Function<R(Args...)>> _functions;

    public:
        void clear() { _functions.clear(); }

        operator bool() const { return !_functions.empty(); }

        R operator()(Args... args) const
        {
            if constexpr (types::IsVoid<R>)
            {
                for (auto &f : _functions)
                    f(FORWARD(args)...);
            }
            else
            {
                if (_functions.empty())
                    RUNTIME_ERROR("Cannot call empty multifunction");

                auto merge = MergeOperator();
                R result = _functions.front()(FORWARD(args)...);

                for (size_t i = 1; i < _functions.size(); i++)
                    result = merge(result, _functions[i](FORWARD(args)...));

                return result;
            }
        }

        template<std::invocable<Args...> Callable>
        void append(Callable &&c)
        {
            _functions.emplace_back(FORWARD(c));
        }
    };

    template<class... Args>
    using Multievent = BasicMultifunction<void, void, Args...>;

    template<class... Args>
    using Multipredicate = BasicMultifunction<std::logical_and<>, bool, Args...>;

    template<class T>
    constexpr auto implicit_cast = [](auto &&x) -> T { return FORWARD(x); };
}