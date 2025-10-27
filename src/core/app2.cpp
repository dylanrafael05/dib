#ifdef DIB_APP2

#include <unordered_map>
#include <memory>
#include <iostream>
#include <filesystem>
#include <algorithm>

#include "dib/env.h"
#include "dib/app2.lib.h"
#include "dib/project.h"
#include "dib/hotreload.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

using namespace dib::app2;
using namespace dib::resources;
using namespace dib::ecs;
using namespace dib;

namespace fs = std::filesystem;

// TODO: redo this //

// Modifiers //
System System::in_group(dib::strings::string_literal name) &&
{
    parent = name;
    return *this;
}

System System::order_after(dib::strings::string_literal sys) &&
{
    order = sys;
    order_is_before = false;
    return *this;
}

System System::order_before(dib::strings::string_literal sys) &&
{
    order = sys;
    order_is_before = true;
    return *this;
}

System System::run_if(PredicateCallback &&pred) &&
{
    predicate = std::move(pred);
    return *this;
}

void System::mark_persistent()
{
    persistent = true;
}

// Constructors //
System System::simple(dib::strings::string_literal name, Callback &&function)
{
    System system;
    
    system.function = std::move(function);
    system.name = name;

    return system;
}

System System::group(dib::strings::string_literal name, Callback &&prefix, Callback &&suffix)
{
    System system;
    system.function = nullptr;
    system.name = name;
    system.prefix = std::move(prefix);
    system.suffix = std::move(suffix);

    return system;
}

// Setup systems //
enum class ConStatus : unsigned char
{
    none = 0,
    in_progress = 1,
    complete = 2
};

struct SystemInfo
{
    System sys;
    std::vector<SystemInfo*> subsystems;
    long order = 0;
    ConStatus kinship_stat = ConStatus::none;
    ConStatus order_stat = ConStatus::none;

    SystemInfo() {}

    SystemInfo(const System &sys)
        : sys(sys)
    {}
};

using system_map = std::unordered_map<std::string_view, SystemInfo>;

void populate_map(const std::vector<System> &sys, system_map &map)
{
    for(auto &system : sys)
    {
        if(map.count(system.name))
        {
            std::cerr << "Multiple systems are defined with the name " << system.name.c_str() << std::endl;
            std::abort();
        }

        map[system.name] = system;
    }
}

void calculate_kinship(system_map &map, std::vector<SystemInfo*> &bases, SystemInfo &sys)
{
    if(sys.sys.parent == "")
    {
        bases.push_back(&sys);
        sys.kinship_stat = ConStatus::complete;

        return;
    }

    if(map.count(sys.sys.parent) == 0)
    {
        std::cerr << "The system group " << sys.sys.parent.c_str() << " does not exist.";
        std::abort();
    }

    map[sys.sys.parent].subsystems.push_back(&sys);
    sys.kinship_stat = ConStatus::complete;
}

void calculate_all_orders(system_map &map, std::vector<SystemInfo*> &systems);
void calculate_order(system_map &map, SystemInfo &system)
{
    if(system.sys.order == "")
    {
        if(system.sys.parent == "")
        {
            system.order = 0;
        }
        else 
        {
            auto &parent = map[system.sys.parent];
            system.order = parent.order + parent.subsystems.size() + 2;
        }

        system.order_stat = ConStatus::complete;
        calculate_all_orders(map, system.subsystems);

        return;
    }

    system.order_stat = ConStatus::in_progress;

    auto it = map.find(system.sys.order.c_str());
    if(it == map.end())
    {
        std::cerr << "The group " << system.sys.order.c_str() << " does not exist.";
        std::abort();
    }

    auto &other = it->second;

    if(other.order_stat == ConStatus::in_progress)
    {
        std::cerr << "Recursive ordering detected, including system named '" << system.sys.name.c_str() << "'." << std::endl;
        std::abort();
    }

    if(other.sys.parent != system.sys.parent.c_str())
    {
        std::cerr << "Ordering specification crosses kinship boundaries." << std::endl;
        std::abort();
    }

    calculate_order(map, other);

    system.order_stat = ConStatus::complete;
    system.order = other.order;

    if(system.sys.order_is_before) system.order--;
    else system.order += other.subsystems.size() * 2 + 1;
    
    calculate_all_orders(map, system.subsystems);
}

void calculate_all_orders(system_map &map, std::vector<SystemInfo*> &systems)
{
    for(auto sys : systems)
    {
        if(sys->order_stat == ConStatus::none)
        {
            calculate_order(map, *sys);
        }
    }
}

void sort_build_calls(std::vector<SystemInfo*> &vec, SystemExecutor &executor);
void build_call(SystemInfo &info, SystemExecutor &executor)
{
    size_t pred_idx = -1;
    if(info.sys.predicate)
    {
        pred_idx = executor.predicates.size();
        executor.predicates.push_back({});
        
        executor.predicates[pred_idx].function = info.sys.predicate;
        executor.predicates[pred_idx].index = executor.functions.size();
    }

    if(info.sys.prefix)
    {
        executor.functions.push_back(info.sys.prefix);
    }

    if(info.sys.function)
    {
        executor.functions.push_back(info.sys.function);
    }
    else 
    {
        sort_build_calls(info.subsystems, executor);
    }

    if(info.sys.suffix)
    {
        executor.functions.push_back(info.sys.suffix);
    }
    
    if(pred_idx != (size_t)(-1))
    {
        executor.predicates[pred_idx].skip_to = executor.functions.size();
    }
}

void sort_build_calls(std::vector<SystemInfo*> &vec, SystemExecutor &executor)
{
    std::sort(vec.begin(), vec.end(), [](const SystemInfo* lhs, const SystemInfo *rhs) {return lhs->order < rhs->order;});
    for(auto sys : vec)
    {
        build_call(*sys, executor);
    }
}

void app2::detail::build_calls(const std::vector<System> &systems, SystemExecutor &executor)
{
    executor.clear();

    system_map map;
    populate_map(systems, map);

    std::vector<SystemInfo*> bases;
    
    for(auto it = map.begin(); it != map.end(); it++)
    {
        calculate_kinship(map, bases, it->second);
    }
    
    calculate_all_orders(map, bases);
    sort_build_calls(bases, executor);
}

void SystemExecutor::execute() const
{
    size_t index = 0;
    size_t pred_index = 0;

    while(index < functions.size())
    {
        if(pred_index != predicates.size() && predicates[pred_index].index == index)
        {
            if(predicates[pred_index].function())
            {
                index = predicates[pred_index].skip_to;
            }
        }

        while(pred_index != predicates.size() && predicates[pred_index].index <= index)
            pred_index++;

        functions[index]();

        index++;
    }
}

void SystemExecutor::clear()
{
    functions.clear();
    predicates.clear();
}

// System management //
App &App::add_system(System &&system)
{
    loop_systems.push_back(std::move(system));
    return *this;
}

App &App::add_systems(std::initializer_list<System> &&system)
{
    for(auto &item : std::move(system))
    {
        loop_systems.push_back(item);
    }

    return *this;
}

App &App::add_init_system(System &&system)
{
    init_systems.push_back(std::move(system));

    return *this;
}

App &App::add_init_systems(std::initializer_list<System> &&system)
{
    for(auto &item : std::move(system))
    {
        init_systems.push_back(item);
    }

    return *this;
}

App &App::add_cleanup_system(System &&system)
{
    cleanup_systems.push_back(std::move(system));

    return *this;
}

App &App::add_cleanup_systems(std::initializer_list<System> &&system)
{
    for(auto &item : std::move(system))
    {
        cleanup_systems.push_back(item);
    }

    return *this;
}

// Modifiers //
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

// Dynamic app info struct //
struct DynamicAppInfo
{
    hotreload::DLL dll;
    std::chrono::file_clock::time_point last_poll;
};

auto get_proper_dll_path()
{
    auto ex = env::executable_path();
    auto fn = ex.filename().replace_extension().string();

    return ex.replace_filename("lib"+fn+".so");
}

void load_game_dll(DynamicAppInfo &dinfo)
{
    auto proper_path = get_proper_dll_path();
    auto current_time = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    auto tmp_path = env::executable_directory_path() / ("_" + current_time + ".so");
    fs::copy_file(proper_path, tmp_path);

    dinfo.dll = {tmp_path};
    dinfo.dll.load();
}

void mark_systems_persistent(auto &all)
{
    for(auto &sys : all)
    {
        sys.mark_persistent();
    }
}

void clean_nonpersistent_systems(auto &all)
{
    std::erase_if(all, [](const System &sys) {return !sys.is_persistent();});
}

App::~App()
{
    if(!_running) return;

    // Perform cleanup //
    if(!_is_static)
    {
        auto &dinfo = *(DynamicAppInfo*)_dynamic_info;

        dinfo.dll.unload();
        fs::remove(dinfo.dll.path());

        delete &dinfo;
    }

    CloseWindow();
}

// Executors //
void App::run(bool is_static)
{
    std::cout << "Running dibapp!" << std::endl;

    this->_running = true;
    this->_is_static = is_static;

    if(!is_static)
    {
        _dynamic_info = new DynamicAppInfo();
    }

    mark_systems_persistent(loop_systems);
    mark_systems_persistent(cleanup_systems);

    InitWindow(window_width, window_height, title.c_str());
    SetTargetFPS(target_fps);

    // Load modules //
    if(!is_static)
    {
        auto &dinfo = *(DynamicAppInfo*)_dynamic_info;
        load_game_dll(dinfo);
    }

    // Build and execute initializer systems //
    {
        SystemExecutor init;
        detail::build_calls(init_systems, init);

        init.execute();
    }

    // Build and store loop systems //
    detail::build_calls(loop_systems, loop_executor);

    // Execute main loop //
    while(!WindowShouldClose())
    {
        loop_executor.execute();

        if(!is_static)
        {
            // Perform hot reloading if necessary //
            auto &dinfo = *(DynamicAppInfo*)_dynamic_info;
            auto proper_path = get_proper_dll_path();

            if(!fs::exists(proper_path))
            {
                std::cout << "Awaiting reload of core library." << std::endl;
                while(!fs::exists(proper_path));
                
                std::cout << "Reloading core library." << std::endl;
                clean_nonpersistent_systems(loop_systems);
                clean_nonpersistent_systems(cleanup_systems);

                dinfo.dll.unload();
                fs::remove(dinfo.dll.path());
                
                load_game_dll(dinfo);

                app2::detail::build_calls(loop_systems, loop_executor);
            }
            
            dinfo.last_poll = std::chrono::file_clock::now();
        }
    }

    // Build and execute cleanup systems //
    {
        SystemExecutor cleanup;
        detail::build_calls(cleanup_systems, cleanup);

        cleanup.execute();
    }
}

void FlushCommands()
{
    scene().commands().flush();
}

App &dib::app2::instance()
{
    static App inst = []
    {
        App value;

        // Add prebuild systems //
        value.add_systems({
            System::group("update", nullptr, &FlushCommands),
            System::group("draw", &BeginDrawing, &EndDrawing).order_after("update")
        });

        return value;
    }();

    return inst;
}

Scene &dib::app2::scene()
{
    static Scene scene;
    return scene;
}

Commands& dib::app2::commands()
{
    return scene().commands();
}

ResourceManager &dib::app2::detail::resource_manager(bool use_batch)
{
    static std::unique_ptr<ResourceManager> man = [&]
    {
        if(use_batch) 
        {
            auto ret = new ResourceBatch();
            ret->open(resource_batch_location());

            return std::unique_ptr<ResourceManager>(ret);
        }

        auto ret = new ResourceFolder(env::executable_directory_path() / ".." / "resources");
        return std::unique_ptr<ResourceManager>(ret);
    }();

    return *man;
}

#endif