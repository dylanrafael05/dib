#pragma once

#include <stddef.h>

#include "dib/conststring.h"
#include "dib/resources/resources.h"
#include "dib/plugins/concept.h"
#include "dib/ecs/entities.h"
#include "dib/ecs/singletons.h"
#include "dib/ecs/state_machine.h"
#include "dib/ecs/world.h"
#include "dib/ecs/systems.h"
#include "dib/app.fwd.h"
#include "dib/ints.h"

// Module helper functions //
#define __dibapp_eval(x) x

#define __file_line_col __FILE__, __LINE__
#define __on_app_load \
    void __on_app_load__(::dib::App &app, ::dib::detail::UniqueIdentifier<__file_line_col>); \
    template<> const char ::dib::detail::UniqueIdentifier<__file_line_col>::value = []{ \
        __on_app_load__(::dib::this_app(), ::dib::detail::UniqueIdentifier<__file_line_col>{}); \
        return 0; \
    }(); \
    void __on_app_load__(::dib::App &app, ::dib::detail::UniqueIdentifier<__file_line_col>)

#define on_app_load __dibapp_eval(__on_app_load)

namespace dib
{
    namespace detail
    {
        template<dib::strings::StringConst, uint_least32_t>
        struct UniqueIdentifier
        {
            static const char value;
        };
    }

    constexpr bool use_batch = IS_FLAG_DEFINED(DIBAPP_BATCH);

    namespace detail
    {
        std::unique_ptr<dib::res::ResourceStore> get_resource_manager(bool use_batch);
    }
    
    App &this_app();

    class App
    {
        ecs::Systems _sys_scheduler;
        ecs::World _world;

        std::string title;
        float target_fps = 0;
        int window_width = 0;
        int window_height = 0;
        bool _running = false;
        bool _wants_close = false;

        App()
        { 
            ecs::detail::initialize_main(_sys_scheduler); 
        }
        ~App();

        void init_resource_manager()
        {
            world().resources().set_store(detail::get_resource_manager(use_batch));
        }

        friend App &dib::this_app();

    public:
        u64 get_frame_counter() const;

        ecs::Systems &systems() { return _sys_scheduler; }
        const ecs::Systems &systems() const { return _sys_scheduler; }
        
        ecs::World &world() { return _world; }
        const ecs::World &world() const { return _world; }

        ecs::Entities &entities() { return _world.entities(); }
        const ecs::Entities &entities() const { return _world.entities(); }
        ecs::Singletons &singletons() { return _world.singletons(); }
        const ecs::Singletons &singletons() const { return _world.singletons(); }
        ecs::Messages &messages() { return _world.messages(); }
        const ecs::Messages &messages() const { return _world.messages(); }
        ecs::StateMachine &state_machine() { return _world.state_machine(); }
        const ecs::StateMachine &state_machine() const { return _world.state_machine(); }
        ecs::Commands &commands() { return _world.commands(); }
        const ecs::Commands &commands() const { return _world.commands(); }
        res::Resources &resources() { return _world.resources(); }
        const res::Resources &resources() const { return _world.resources(); }

        App &set_fps(float target_fps);
        App &set_title(const std::string &title);
        App &set_dimensions(int width, int height);
        App &set_config_flags(int flags);
        App &initialize(int argc, const char *const *argv);

        App &inject(dib::plugins::IsPlugin auto &&plug)
        {
            plug.inject(*this);
            return *this;
        }

        template<class ...State>
        App &set_states(State &&...state)
        {
            (state_machine().init(FORWARD(state)), ...);
            return *this;
        }

		App &add_system(ecs::System &&sys);
		App &add_systems(const std::initializer_list<ecs::System> &systems);

        void run();
        void exit() { _wants_close = true; }
    };

    template<ecs::IsSingleton T>
    inline T &get_singleton() { return singletons().get<T>(); }

    template<ecs::IsComponent... T>
    inline ecs::Query<T...> query() { return entities().query<T...>(); }
}