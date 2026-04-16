#pragma once

#include <unordered_map>
#include <typeindex>

#include "dib/metautils.h"
#include "dib/raw_memory.h"
#include "dib/option.h"
#include "dib/preprocess.h"

namespace dib::ecs
{
    class Messages;
}

namespace dib
{
    ecs::Messages &messages();
}

namespace dib::ecs
{
    class Message {};
    constexpr Message message;

    template<class T>
    concept IsMessage = AnnotatedWith<T, Message>;

    class Messages
    {
    public:
        template<IsMessage T>
        void send(T &&message)
        {
            initialize_storage<T>();
            messages.at(typeid(T)).emplace_back<T>(MOVE(message));
        }

        template<IsMessage T>
        bool has()
        {
            return messages.contains(typeid(T)) && messages.at(typeid(T)).size() != 0;
        }

        template<IsMessage T>
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
        template<IsMessage T>
        void initialize_storage()
        {
            if (messages.contains(typeid(T))) [[likely]] return;
            messages[typeid(T)] = dib::structures::ErasedVec::create<T>();
        }

        std::unordered_map<std::type_index, dib::structures::ErasedVec> messages;
    };
}