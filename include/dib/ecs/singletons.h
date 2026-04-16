#pragma once

#include <unordered_map>

#include "dib/metautils.h"
#include "dib/raw_memory.h"
#include "dib/debug.h"
#include "dib/preprocess.h"

namespace dib::ecs
{
    class Singletons;
}

namespace dib
{
    ecs::Singletons &singletons();
}

namespace dib::ecs
{
    class Singleton {};
    constexpr Singleton singleton;

    template<class T>
    concept IsSingleton = AnnotatedWith<T, Singleton>;

	class Singletons
	{
    public:
        template<IsSingleton T, class... Args>
        Singletons &create(Args &&...args)
        {
            if (has<T>())
            {
                RUNTIME_ERROR("Cannot create multiple instances of singleton type {}.", types::nameof<T>);
            }

            singletons[refl::typeof<T>] = T(FORWARD(args)...);
            return *this;
        }

        template<IsSingleton T, class... Args>
        T &get_new(Args&& ...args)
        {
            if (has<T>())
                return get<T>();

            create<T>(FORWARD(args)...);
            return get<T>();
        }

        template<IsSingleton T>
        T &get()
        {
            if (!has<T>())
            {
                RUNTIME_ERROR("Attempt to get a singleton of type {} which does not exist.", refl::typeof<T>.name());
            }

            return singletons[refl::typeof<T>].template get<T>();
        }

        template<IsSingleton T>
        bool has() const { return singletons.contains(refl::typeof<T>); }

        template<IsSingleton T>
        void remove()
        {
            singletons.erase(refl::typeof<T>);
        }

    private:
        std::unordered_map<refl::Type, dib::structures::ErasedSingleton> singletons;
	};
}