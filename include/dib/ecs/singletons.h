#ifndef __DIB_ECS_SINGLETONS_H
#define __DIB_ECS_SINGLETONS_H

#include <unordered_map>
#include <typeinfo>
#include <typeindex>
#include <optional>

#include "../raw_memory.h"

namespace dib::ecs
{
	class Singletons
	{
    public:
        template<class T, class... Args>
        Singletons &create(Args &&...args)
        {
            if (singletons.count({ typeid(T) }) > 0)
                return *this;

            singletons[{typeid(T)}] = T(std::forward<Args>(args)...);
            return *this;
        }

        template<class T, class... Args>
        T &get_new(Args&& ...args)
        {
            if (singletons.count({ typeid(T) }) != 0)
                return get<T>();

            create<T>(std::forward<Args>(args)...);
            return get<T>();
        }

        template<class T>
        T &get()
        {
            #ifndef NDEBUG
            if (singletons.count({ typeid(T) }) == 0)
            {
                std::cerr << "Attempted to get a singleton which does not exist." << std::endl;
                std::abort();
            }
            #endif

            return singletons[{typeid(T)}].template get<T>();
        }

        template<class T>
        bool has() const { return singletons.contains({ typeid(T) }); }

        template<class T>
        void remove()
        {
            singletons.erase({ typeid(T) });
        }

    private:
        std::unordered_map<std::type_index, dib::structures::ErasedSingleton> singletons;
	};

    class Messages
    {
    public:
        template<class T>
        void send(T &&message)
        {
            initialize_storage<T>();
            messages.at(typeid(T)).emplace_back<T>(std::move(message));
        }

        template<class T>
        bool has()
        {
            return messages.contains(typeid(T)) && messages.at(typeid(T)).size() != 0;
        }

        template<class T>
        std::optional<T> get()
        {
            // Early exit for empty queue //
            if (!messages.contains(typeid(T))) return std::nullopt;
            auto &stack = messages.at(typeid(T));

            // Early exit for empty queue //
            if (stack.size() == 0) return std::nullopt;

            T &element = stack.get<T>(stack.size() - 1);
            T ret = std::move(element);

            stack.pop_back();

            return { std::move(ret) };
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

#endif