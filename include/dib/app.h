#pragma once

#include <stddef.h>

#include "dib/conststring.h"
#include "dib/resources.h"
#include "dib/plugins/concept.h"
#include "dib/ecs/components.h"
#include "dib/ecs/singletons.h"
#include "dib/ecs/state_machine.h"
#include "dib/ecs/world.h"
#include "dib/ecs/systems.h"
#include "dib/app.fwd.h"

// Module helper functions //
#define __dibapp_eval(x) x

#define __file_line_col __FILE__, __LINE__
#define __on_app_load \
    void __on_app_load__(::dib::app::UniqueIdentifier<__file_line_col>); \
    template<> const char ::dib::app::UniqueIdentifier<__file_line_col>::value = []{ \
        __on_app_load__(::dib::app::UniqueIdentifier<__file_line_col>{}); \
        return 0; \
    }(); \
    void __on_app_load__(::dib::app::UniqueIdentifier<__file_line_col>)

#define on_app_load __dibapp_eval(__on_app_load)

namespace dib::app
{
    template<dib::strings::StringConst, uint_least32_t>
    struct UniqueIdentifier
    {
        static const char value;
    };

#ifdef DIBAPP_BATCH
    constexpr bool use_batch = true;
#else
    constexpr bool use_batch = false;
#endif

    namespace detail
    {
        std::unique_ptr<dib::resources::ResourceStore> get_resource_manager(bool use_batch);
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

        App()
        { 
            ecs::detail::initialize_main(_sys_scheduler); 
        }
        ~App();

        void init_resource_manager()
        {
            world().resources().set_store(detail::get_resource_manager(use_batch));
        }

        friend App &dib::app::this_app();

    public:
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
        resources::Resources &resources() { return _world.resources(); }
        const resources::Resources &resources() const { return _world.resources(); }

        App &set_fps(float target_fps);
        App &set_title(const std::string &title);
        App &set_dimensions(int width, int height);
        App &initialize(int argc, const char *const *argv);

        template<dib::plugins::IsPlugin Plugin>
        App &inject(Plugin &&plug)
        {
            plug.inject(*this);
            return *this;
        }

        template<dib::plugins::IsPlugin Plugin>
        App &inject()
        {
            return inject(Plugin());
        }

        void run();
    };
}