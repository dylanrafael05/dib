#ifndef __DIBAPP2_H
#define __DIBAPP2_H

#include <source_location>
#include <stddef.h>
#include <span>
#include <vector>

#include "conststring.h"
#include "functional.h"
#include "resources.h"
#include "app2.fwd.h"
#include "plugins/concept.h"
#include "io/os.h"

// Module helper functions //
#define __dibapp2_eval(x) x

#define __file_line_col __FILE__, __LINE__
#define __on_game_load \
    void __on_game_load__(::dib::app2::unique_identifier<__file_line_col>); \
    template<> const char ::dib::app2::unique_identifier<__file_line_col>::value = []{ \
        __on_game_load__(::dib::app2::unique_identifier<__file_line_col>{}); \
        return 0; \
    }(); \
    void __on_game_load__(::dib::app2::unique_identifier<__file_line_col>)

#define on_game_load __dibapp2_eval(__on_game_load)

namespace dib::app2
{
    template<dib::strings::StringConst, uint_least32_t>
    struct unique_identifier
    {
        static const char value;
    };

    //! Deprecated: no more modules! all hail makefile!
    // void set_current_module(dib::strings::string_literal);
    
    #ifdef DIBAPP_BATCH
    constexpr bool use_batch = true;
    #else
    constexpr bool use_batch = false;
    #endif
    
    #if defined(DIBAPP_STATIC) | DIB_OS_WIN
    constexpr bool is_static = true;
    #else
    constexpr bool is_static = false;
    #endif
    
    using Callback = dib::functional::SmallFunction<void(void)>;
    using PredicateCallback = dib::functional::SmallFunction<bool(void)>;

    struct System
    {
    private:
        bool persistent = false;
        friend App &instance();

    public:
        System() {}

        Callback function = nullptr;
        Callback prefix = nullptr;
        Callback suffix = nullptr;
        PredicateCallback predicate = nullptr;
        dib::strings::StringLiteral name;
        dib::strings::StringLiteral parent;
        dib::strings::StringLiteral order;
        bool order_is_before = false;

        bool is_persistent() const {return persistent;}
        void mark_persistent();

        System in_group(dib::strings::StringLiteral) &&;
        System order_after(dib::strings::StringLiteral) &&;
        System order_before(dib::strings::StringLiteral) &&;
        System run_if(PredicateCallback &&) &&;

        static System simple(dib::strings::StringLiteral name, Callback &&function);
        static System group(dib::strings::StringLiteral name, Callback &&prefix = nullptr, Callback &&suffix = nullptr);
    };
    
    namespace detail
    {
        struct Predicate
        {
            PredicateCallback function;
            size_t skip_to;
            size_t index;
        };
    }

    struct SystemExecutor
    {
        std::vector<Callback> functions;
        std::vector<detail::Predicate> predicates;

        bool is_initializing = false;
        bool is_updating = false;

        void execute() const;
        void clear();
    };

    namespace detail
    {
        void build_calls(const std::vector<System> &sys, SystemExecutor &exec);
    }

    class App
    {
        SystemExecutor loop_executor;
        std::vector<System> loop_systems;
        std::vector<System> cleanup_systems;
        std::vector<System> init_systems;

        std::string title;
        float target_fps = 0;
        int window_width = 0;
        int window_height = 0;
        
        void *_dynamic_info = nullptr;
        bool _is_static = false;
        bool _running = false;

        App() {}
        ~App();

        friend App &instance();

        void run(bool is_static);

    public:
        App &add_init_system(System &&system);
        App &add_init_systems(std::initializer_list<System> &&system);
        
        App &add_cleanup_system(System &&system);
        App &add_cleanup_systems(std::initializer_list<System> &&system);

        App &add_system(System &&system);
        App &add_systems(std::initializer_list<System> &&system);

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

        void run();
    };
    
    namespace detail
    {
        dib::res::Resources &resource_manager(bool use_batch);
    }
    
    App &instance();
    dib::ecs::Scene &scene();
    dib::ecs::Commands& commands();
    extern dib::res::Resources &resource_manager();
}

#endif