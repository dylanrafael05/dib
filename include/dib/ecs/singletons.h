#pragma once

#include <unordered_map>
#include <typeindex>

#include "dib/raw_memory.h"
#include "dib/option.h"
#include "dib/debug.h"
#include "dib/preprocess.h"
#include "dib/types.h"

namespace dib::ecs
{
    struct Singletons;
    struct Messages;
}

namespace dib
{
    ecs::Singletons &singletons();
    ecs::Messages &messages();
}

namespace dib::ecs
{
	class Singletons
	{
    public:
        template<class T, class... Args>
        Singletons &create(Args &&...args)
        {
            if (has<T>())
            {
                RUNTIME_ERROR("Cannot create multiple instances of singleton type {}.", types::nameof<T>);
            }

            singletons[refl::typeof<T>] = T(FORWARD(args)...);
            return *this;
        }

        template<class T, class... Args>
        T &get_new(Args&& ...args)
        {
            if (has<T>())
                return get<T>();

            create<T>(FORWARD(args)...);
            return get<T>();
        }

        template<class T>
        T &get()
        {
            if (!has<T>())
            {
                RUNTIME_ERROR("Attempt to get a singleton which does not exist.");
            }

            return singletons[refl::typeof<T>].template get<T>();
        }

        template<class T>
        bool has() const { return singletons.contains(refl::typeof<T>); }

        template<class T>
        void remove()
        {
            singletons.erase(refl::typeof<T>);
        }

    private:
        std::unordered_map<refl::Type, dib::structures::ErasedSingleton> singletons;
	};

    class Messages
    {
    public:
        template<class T>
        void send(T &&message)
        {
            initialize_storage<T>();
            messages.at(typeid(T)).emplace_back<T>(MOVE(message));
        }

        template<class T>
        bool has()
        {
            return messages.contains(typeid(T)) && messages.at(typeid(T)).size() != 0;
        }

        template<class T>
        dib::option::Option<T> get()
        {
            // Early exit for empty queue //
            if (!messages.contains(typeid(T))) return dib::option::none;
            auto &stack = messages.at(typeid(T));

            // Early exit for empty queue //
            if (stack.size() == 0) return dib::option::none;

            T &element = stack.get<T>(stack.size() - 1);
            T ret = MOVE(element);

            stack.pop_back();

            return { MOVE(ret) };
        }

    private:
        template<class T>
        void initialize_storage()
        {
            if (messages.contains(typeid(T))) [[likely]] return;
            messages[typeid(T)] = dib::structures::ErasedVec::create<T>();
        }

        std::unordered_map<std::type_index, dib::structures::ErasedVec> messages;
    };
}