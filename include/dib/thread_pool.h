#pragma once

#include "dib/functional.h"
#include "dib/preprocess.h"
#include "dib/reflect.h"
#include "dib/types.h"
#include "dib/vector.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

namespace dib::threading
{
    class ThreadScheduler
    {
        struct ThreadBlock
        {
            std::thread thread;

            dib::functional::fn<void(void *)> function;
            refl::Type args_type;
            void *args = nullptr;
            size_t args_size = 0;

            mutable std::mutex mutex;
            std::condition_variable cv;

            std::atomic_bool completed = false;
        };

        size_t _thread_count;
        dib::structures::DynArray<ThreadBlock> _threads;
        std::atomic_uint _threads_free;
        std::mutex _thread_completed_mutex;
        std::condition_variable _thread_completed;
        bool _tearing_down;

        static void thread_fn(ThreadScheduler *sched, ThreadBlock *self)
        {
            self->completed = false;

            // TODO! figure out why this does not work.
            while(true)
            {
                std::unique_lock lock(self->mutex);
                self->cv.wait(lock);

                if(sched->_tearing_down)
                    break;

                self->function(self->args);
                self->function = nullptr;
                self->args_type.destruct(self->args);
                
                // SAFETY; assignment cannot be reordered against function call
                self->completed.store(true, std::memory_order_release);
                {
                    std::unique_lock lock(sched->_thread_completed_mutex);
                    sched->_threads_free.fetch_add(1, std::memory_order_release);
                    sched->_thread_completed.notify_one();
                }
            }
        }

    public:
        explicit ThreadScheduler(size_t thread_count = 1'000)
            : _thread_count(std::max(
                (size_t)std::min(
                    (double)thread_count, 
                    (std::thread::hardware_concurrency() - 1) * 0.8
                ), 
                1uz
            ))
            , _threads(_thread_count)
            , _threads_free(_thread_count)
            , _thread_completed()
            , _tearing_down(false)
        {
            for(size_t i = 0; i < _threads.size(); i++)
            {
                _threads[i].thread = std::thread(
                    &thread_fn, 
                    this, &_threads[i]);
            }
        }

        ThreadScheduler(const ThreadScheduler &) = delete;
        ThreadScheduler(ThreadScheduler &&) = delete;

        ~ThreadScheduler()
        {
            _tearing_down = true;

            // Signal to all condition variables
            for(size_t i = 0; i < _threads.size(); i++)
                _threads[i].cv.notify_all();

            // Wait for all threads to end
            for(size_t i = 0; i < _threads.size(); i++)
            {
                _threads[i].thread.join();

                if(_threads[i].args)
                    delete[] (char *)_threads[i].args;
            }
        }

        template<class ...Args>
        void execute(
            std::type_identity_t<dib::functional::Function<void(Args...)>> fn, 
            Args ...args)
        {
            // Block until there is at least one free thread.
            if(_threads_free.load(std::memory_order_acquire) == 0)
            {
                std::unique_lock wait_lock(_thread_completed_mutex);
                _thread_completed.wait(wait_lock, [&] 
                { 
                    return _threads_free > 0; 
                });
            }

            // Find the free thread, which must exist at this point
            for(size_t i = 0; i < _threads.size(); i++)
            {
                if(_threads[i].completed.load(std::memory_order_acquire))
                {
                    using ArgsTuple = std::tuple<
                        dib::functional::Function<void(Args...)>, Args...>;

                    auto &thread = _threads[i];

                    // Resize the thread's argument buffer to fit the new
                    // arguments provided here.
                    if(thread.args_size < sizeof(ArgsTuple))
                    {
                        thread.args_size = sizeof(ArgsTuple);
                        thread.args = new char[thread.args_size];
                    }

                    // Create the arguments.
                    new(thread.args) ArgsTuple(MOVE(fn), MOVE(args)...);
                    thread.args_type = refl::typeof<ArgsTuple>;

                    // Create the function which takes args via the given buffer
                    _threads[i].function = +[](void *buffer)
                    {
                        auto &tpl = *(ArgsTuple *)buffer;
                        auto &fn = std::get<0>(tpl);

                        [&]<size_t ...I>(std::index_sequence<I...>)
                        {
                            fn(MOVE(std::get<I + 1>(tpl))...);
                        }
                        (std::make_index_sequence<sizeof...(Args)>{});
                    };
                    
                    _threads_free.fetch_sub(1, std::memory_order_release);
                    thread.cv.notify_all();

                    break;
                }
            }
        }

        void wait_for_complete()
        {
            // Wait until all threads are completed
            std::unique_lock wait_lock(_thread_completed_mutex);
            _thread_completed.wait(wait_lock, [&] 
            { 
                return _threads_free == _thread_count; 
            });
        }
    };  
}