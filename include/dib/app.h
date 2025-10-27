#ifndef __DIB_ECS_APP_H
#define __DIB_ECS_APP_H

#include "dib/ecs/world.h"
#include "dib/ecs/systems.h"

#include <source_location>
#include <stddef.h>
#include <span>
#include <vector>

#include "conststring.h"
#include "function_util.h"
#include "resources.h"
#include "plugins/concept.h"
#include "io/os.h"

// Module helper functions //
#define __dibapp_eval(x) x

#define __file_line_col __FILE__, __LINE__
#define __on_app_load \
    void __on_app_load__(::dib::app::unique_identifier<__file_line_col>); \
    template<> const char ::dib::app::unique_identifier<__file_line_col>::value = []{ \
        __on_app_load__(::dib::app::unique_identifier<__file_line_col>{}); \
        return 0; \
    }(); \
    void __on_app_load__(::dib::app::unique_identifier<__file_line_col>)

#define on_app_load __dibapp_eval(__on_app_load)

namespace dib::app
{
    template<dib::strings::string_const, uint_least32_t>
    struct unique_identifier
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
        std::unique_ptr<dib::resources::Resources> get_resource_manager(bool use_batch);
    }

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
            world().set_resource_manager(detail::get_resource_manager(use_batch));
        }

        friend App &instance();

    public:
        ecs::Systems &systems() { return _sys_scheduler; }
        const ecs::Systems &systems() const { return _sys_scheduler; }
        ecs::World &world() { return _world; }
        const ecs::World &world() const { return _world; }

        App &set_fps(float target_fps);
        App &set_title(const std::string &title);
        App &set_dimensions(int width, int height);
        App &initialize(int argc, const char *const *argv);

        template<dib::plugins::plugin Plugin>
        App &inject(Plugin &&plug)
        {
            plug.inject(*this);
            return *this;
        }

        template<dib::plugins::plugin Plugin>
        App &inject()
        {
            return inject(Plugin());
        }

        void run();
    };

    App &instance();
}

#endif