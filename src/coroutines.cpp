#include "dib/coroutines.h"
#include "dib/app.h"
#include "dib/ecs/entities.h"
#include "raylib.h"
#include <coroutine>

using namespace dib;
using namespace dib::ecs;
using namespace dib::coro;
using namespace dib::functional;

namespace cd = dib::coro::detail;

bool Coroutine::is_completed() const 
{ 
    return associated_runner.is_alive() && !handle.promise().completed; 
}
void Coroutine::cancel() 
{ 
    handle.promise().completed = true;
    commands().destroy_entity(associated_runner);
}

std::suspend_always cd::Promise::yield_value(CoroutineAwaiter &&awaiter)
{
    this->awaiter = MOVE(awaiter);
    return {};
}

std::suspend_always cd::Promise::yield_value(Coroutine coro)
{
    auto awaiter = wait_until([&] { return coro.is_completed(); });
    return yield_value(MOVE(awaiter));
}

void dib::coro::run_coroutines()
{
    dib::query<cd::CoroutineRunner>().for_each_sync([](EntityID en, cd::CoroutineRunner &runner)
    {
        auto &promise = runner.coro.handle.promise();
        
        if(auto awaiter = promise.awaiter.try_get(); !awaiter || awaiter->can_resume())
            runner.coro.handle.resume();

        if(runner.coro.handle.promise().completed)
            commands().destroy_entity(en);
    });
}

// TODO; we shouldn't need to allocate every time we return this!
CoroutineAwaiter dib::coro::wait_next_frame()
{
    struct WaitNextFrame : public CoroutineAwaiter::Impl
    {
        bool can_resume() override
        {
            return true;
        }
    };

    return { std::make_unique<WaitNextFrame>() };
}

CoroutineAwaiter dib::coro::wait_seconds(float seconds)
{
    struct WaitSeconds : public CoroutineAwaiter::Impl
    {
        WaitSeconds(float start, float duration)
            : start(start)
            , duration(duration)
        {}

        float start;
        float duration;

        bool can_resume() override
        {
            return GetTime() - start >= duration;
        }
    };

    return { std::make_unique<WaitSeconds>(GetTime(), seconds) };
}

CoroutineAwaiter dib::coro::wait_until(Function<bool()> &&pred)
{
    struct WaitUntil : public CoroutineAwaiter::Impl
    {
        WaitUntil(Function<bool()> &&pred)
            : pred(MOVE(pred))
        {}

        Function<bool()> pred;

        bool can_resume() override
        {
            return pred();
        }
    };
    
    return { std::make_unique<WaitUntil>(MOVE(pred)) };
}

Coroutine test()
{
    co_yield wait_next_frame();
}