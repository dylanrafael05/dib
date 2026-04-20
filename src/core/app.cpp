#include "dib/app.h"
#include "dib/ecs/systems_fwd.h"
#include "dib/env.h"
#include "raylib.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wenum-compare"
#pragma GCC diagnostic ignored "-Wenum-enum-conversion"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#pragma GCC diagnostic pop

using namespace dib;
using namespace dib::res;

App &App::set_config_flags(int flags)
{
    SetConfigFlags(flags);
    return *this;
}
App &App::set_fps(float target_fps)
{
    this->target_fps = target_fps;
    return *this;
}
App &App::set_title(const std::string &title)
{
    this->title = title;
    return *this;
}
App &App::set_dimensions(int width, int height)
{
    window_width = width;
    window_height = height;
    return *this;
}

App &App::initialize(int argc, const char *const *argv)
{
    env::initialize(argc, argv);
    return *this;
}

App::~App()
{
    if (!_running) return;
}

void App::run()
{
    this->_running = true;
    init_resource_manager();
    systems().build();

    // Inject our custom resource management into raylib
    SetLoadFileDataCallback(res::detail::file_load_data_callback);
    SetLoadFileTextCallback(res::detail::file_load_text_callback);

    // Initialize raylib
    if(target_fps > 0) 
        SetTargetFPS(target_fps);
    
    SetTraceLogLevel(LOG_ALL);
    InitWindow(window_width, window_height, title.c_str());
    SetTraceLogLevel(LOG_INFO);
    SetExitKey(0);

    if(!IsWindowReady())
    {
        RUNTIME_ERROR("You're not ready for this.");
    }

    // Execute initializer systems
    systems().execute(world(), ecs::groups::OnInit);
    systems().execute(world(), ecs::groups::Start);

    // Execute main loop
    while (!WindowShouldClose() && !_wants_close)
    {
        systems().execute(world(), ecs::groups::Main);
    }

    // Build and execute cleanup systems //
    systems().execute(world(), ecs::groups::OnDeinit);
    
    // Perform cleanup //
    CloseWindow();
    ::exit(0);
}

App &dib::this_app()
{
    static App app;
    return app;
}

std::unique_ptr<ResourceStore> dib::detail::get_resource_manager(bool use_batch)
{
    if (use_batch)
    {
        auto ret = new ResourceBatch();
        ret->open(resource_batch_location());

        return std::unique_ptr<ResourceStore>(ret);
    }
    else
    {
        auto ret = new ResourceFolder(env::executable_directory_path() / "resources");
        return std::unique_ptr<ResourceStore>(ret);
    }
}

App &dib::App::add_system(ecs::System &&sys) 
{ 
    this->systems().add(MOVE(sys));
    return *this;
}

App &dib::App::add_systems(const std::initializer_list<ecs::System> &systems)
{ 
    this->systems().add(systems);
    return *this;
}

ecs::Systems &dib::systems() { return this_app().systems(); }
ecs::Singletons &dib::singletons() { return this_app().singletons(); }
ecs::Commands &dib::commands() { return this_app().commands(); }
ecs::Entities &dib::entities() { return this_app().entities(); }
ecs::StateMachine &dib::state_machine() { return this_app().state_machine(); }
ecs::Messages &dib::messages() { return this_app().messages(); }
res::Resources &dib::resources() { return this_app().resources(); }