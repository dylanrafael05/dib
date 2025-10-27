#include "dib/app.h"
#include "dib/env.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

using namespace dib;
using namespace dib::app;
using namespace dib::resources;

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

    // Perform cleanup //
    CloseWindow();
}

void App::run()
{
    std::cout << "Running dibapp!" << std::endl;

    this->_running = true;
    init_resource_manager();
    systems().build();

    InitWindow(window_width, window_height, title.c_str());
    SetTargetFPS(target_fps);

    // Build and execute initializer systems //
    systems().execute(world(), ecs::groups::OnInit);
    systems().execute(world(), ecs::groups::Start);

    // Execute main loop //
    while (!WindowShouldClose())
    {
        systems().execute(world(), ecs::groups::Main);
    }

    // Build and execute cleanup systems //
    systems().execute(world(), ecs::groups::OnDeinit);
}

App &dib::app::instance()
{
    static App app;
    return app;
}

std::unique_ptr<Resources> dib::app::detail::get_resource_manager(bool use_batch)
{
    if (use_batch)
    {
        auto ret = new ResourceBatch();
        ret->open(resource_batch_location());

        return std::unique_ptr<Resources>(ret);
    }
    else
    {
        auto ret = new ResourceFolder(env::executable_directory_path() / ".." / "resources");
        return std::unique_ptr<Resources>(ret);
    }
}