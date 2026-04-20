#pragma once

#include "dib/option.h"
#include "dib/ecs/entities.h"

#include <coroutine>
#include <memory>

namespace dib::coro
{
    void run_coroutines();

    // Pre-declare promise type //
    namespace detail
    {
        struct Promise;
    }

    /// A handle for a currently executing coroutine.
    struct Coroutine
    {
        using promise_type = detail::Promise;

    private:
        Coroutine(detail::Promise &promise) 
            : handle(std::coroutine_handle<detail::Promise>::from_promise(promise))
        {}

        bool initializing;
        std::coroutine_handle<detail::Promise> handle;
        ecs::EntityID associated_runner;

        friend struct detail::Promise;
        friend void dib::coro::run_coroutines();

    public:
        bool is_completed() const;
        void cancel();
    };
    
    struct CoroutineAwaiter
    {
        struct Impl
        {
            virtual ~Impl() {}
            virtual bool can_resume() = 0;
        };

        CoroutineAwaiter(std::unique_ptr<Impl> &&impl)
            : implementation(MOVE(impl))
        {}

    private:        
        std::unique_ptr<Impl> implementation;

    public:
        bool can_resume() { return implementation->can_resume(); }
    };

    namespace detail
    { 
        struct [[=ecs::component]] CoroutineRunner
        {
            Coroutine coro;
        };
        
        struct Promise
        {
            dib::option::Option<CoroutineAwaiter> awaiter;
            bool completed;

            Coroutine get_return_object() 
            {
                auto result = Coroutine(*this);
                result.initializing = true;

                commands().create_entity(CoroutineRunner
                {
                    .coro = result
                });

                return result;
            }

            std::suspend_always initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() { completed = true; }
            void unhandled_exception() {}

            std::suspend_always yield_value(CoroutineAwaiter &&awaiter);
            std::suspend_always yield_value(Coroutine coro);
        };
    }

    CoroutineAwaiter wait_next_frame();
    CoroutineAwaiter wait_seconds(float seconds);
    CoroutineAwaiter wait_until(functional::Function<bool()> &&pred);
}