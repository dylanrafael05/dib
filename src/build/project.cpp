#include <iostream>
#include <fstream>

#include "dib/project.h"
#include "dib/json.h"
#include "dib/resources/resources.h"

#include <algorithm>

using namespace dib;
using namespace dib::project;
namespace fs = std::filesystem;

CommandBuilder &CommandBuilder::operator<<(const std::string_view &text)
{
    _text += ' ';
    _text += text;

    return *this;
}

void CommandBuilder::execute() const
{
    std::cerr << "Executing command: " << _text << std::endl;

    if(auto ec = std::system(_text.data()); ec != 0)
    {
        std::exit(ec);
    }
    
    std::cerr << "Successful!" 
              << std::endl 
              << std::endl;
}

#define LOAD_ERR                                                                   \
    {                                                                              \
        std::cerr << "Invalid configuration file @line=" << __LINE__ << std::endl; \
        std::abort();                                                              \
    }

fs::path Project::scripts_folder() const
{
    return _dir / "scripts";
}

fs::path Project::build_folder() const
{
    return _dir / "build";
}

fs::path Project::resources_folder() const
{
    return _dir / "resourses";
}

fs::path Project::config_file() const
{
    return _dir / "config.json";
}

void Project::load(const fs::path &dir)
{
    _dir = dir;
    
    std::ifstream config_file(this->config_file());
    if(!config_file) LOAD_ERR;

    // Load configuration //
    json::Any config_raw;
    json::read(config_file, config_raw);

    auto config = config_raw.as_object();
    if(!config) LOAD_ERR;

    if(auto flags = config->get_or_null("flags"))
    {
        auto flags_arr = flags->as_array();
        if(!flags_arr) LOAD_ERR;

        for(auto &flag : *flags_arr)
        {
            auto flag_str = flag.as_string();
            if(!flag_str) LOAD_ERR;
            
            _flags.push_back(*flag_str);
        }
    }

    // Load modules //
    for(auto &dir : fs::directory_iterator{scripts_folder()})
    {
        if(dir.is_directory())
        {
            auto mod = new Module(*this, dir.path());

            _modules.emplace_back(mod);
            _by_name.insert_or_assign(mod->name(), mod);
        }
    }

    // Load dependencies //
    for(auto &mod : _modules)
    {
        mod->load_configuration();
    }

    // Sort by calculated order //
    std::sort(_modules.begin(), _modules.end(), detail::ModuleComparator{});
}

bool dib::project::detail::ModuleComparator::operator()(const std::unique_ptr<Module> &lhs, const std::unique_ptr<Module> &rhs) const
{
    return lhs->_sorting_order < rhs->_sorting_order;
}

std::string module_name_from_path(const fs::path &proj, const fs::path &path)
{
    return fs::relative(path / "mod.cpp", proj).string();
}

Module::Module(Project &project, const std::filesystem::path &dir)
    : _project(&project), _dir(dir)
{
    _name = module_name_from_path(project._dir, dir);
    _identifier = dir.filename().string();
}

fs::path Module::config_file() const
{
    return _dir / "config.json";
}

fs::path Module::source_folder() const
{
    return _dir;
}

fs::path Module::dll_file(const std::filesystem::path &build, bool is_static) const
{
    return build / ("lib" + _dir.parent_path().filename().replace_extension(is_static ? ".o" : ".so").string());
}

enum ModuleStatus
{
    ModuleBlank = 0,
    ModuleInProgress = 1,
    ModuleBuilt = 2
};

void Module::load_configuration()
{
    if(_status == ModuleBuilt) return;

    std::ifstream config_file(this->config_file());

    json::Any config_raw;
    json::read(config_file, config_raw);

    auto config = config_raw.as_object();
    if(!config) LOAD_ERR;

    _status = ModuleInProgress;

    if(auto deps = config->get_or_null("dependencies"))
    {
        auto deps_arr = deps->as_array();
        if(!deps_arr) LOAD_ERR;

        for(auto &dep : *deps_arr)
        {
            // Parse depedency strings //
            auto dep_rstr = dep.as_string();
            if(!dep_rstr) LOAD_ERR;

            auto dep_str = module_name_from_path(_project->_dir, _project->scripts_folder() / *dep_rstr);
            auto dep_it = _project->_by_name.find(dep_str);

            if(dep_it == _project->_by_name.end())
            {
                std::cerr << "Unknown dependency '" << *dep_rstr << "'" << std::endl;
                std::abort();
            }

            auto &dep_inst = dep_it->second;

            if(dep_inst->_status == ModuleInProgress)
            {
                std::cerr << "Circular dependency including module '" << _dir << "' detected." << std::endl;
                std::abort();
            }

            dep_inst->load_configuration();
            _dependencies.push_back(dep_inst);

            // Update sorting order //
            _sorting_order = std::max(_sorting_order, dep_inst->_sorting_order + 1);
        }
    }

    _status = ModuleBuilt;
}

// BUILDING //
void handle_common_flags(CommandBuilder &cmd, const BuildSettings &settings)
{
    // Add include paths //
    cmd << "-I" << settings.dib_path.string()
        << "-I" << (settings.dib_path / "include").string()
        << "-I" << settings.raylib_path.string()
        << "-I" << (settings.raylib_path / "src").string();

    // Add linker paths //
    cmd << "-L" << (settings.dib_path / "src").string()
        << "-L" << (settings.raylib_path / "src").string();

    cmd << "-ldibapp"
        << "-lraylib";

    // Add general flags //
    cmd << "-std=c++20";
    
    // Custom flags //
    for(auto &flag : settings.added_flags)
    {
        cmd << flag;
    }

    // Defines //
    if(settings.static_link)
    {
        cmd << "-DDIBAPP_STATIC";
    }

    if(settings.use_batch)
    {
        cmd << "-DDIBAPP_BATCH";
    }
}

void Module::build(const BuildSettings &settings, const fs::path &output) const
{
    CommandBuilder cmd("g++");
    const char *lib_ext = (settings.static_link ? ".o" : ".so");
    
    // Load all source files //
    for(auto &file : fs::recursive_directory_iterator{_dir})
    {
        if(file.is_regular_file() && file.path().extension() == ".cpp")
        {
            cmd << file.path().string();
        }
    }

    // Common flags //
    handle_common_flags(cmd, settings);

    // Link with build folder //
    cmd << "-L" << output.string();

    // Link with dependencies //
    for(auto dep : _dependencies)
    {
        cmd << "-l" << "lib"+dep->identifier()+lib_ext;
    }

    // Setup build mode //
    if(settings.static_link)
    {
        cmd << "-static";
    }
    else 
    {
        cmd << "-shared"
            << "-fPIC";
    }

    // Output flag //
    cmd << "-o" << (output / ("lib"+identifier()+lib_ext)).string();
    cmd.execute();
}

void Project::build(const BuildSettings &settings, const fs::path &output) const
{
    // Build all modules //
    for(auto &mod : _modules)
    {
        mod->build(settings, output);
    }

    CommandBuilder cmd("g++");

    // Load all source files //
    for(auto &file : fs::directory_iterator{_dir})
    {
        if(file.is_regular_file() && file.path().extension() == ".cpp")
        {
            cmd << file.path().string();
        }
    }

    // Link with modules (if in static mode) //
    if(settings.static_link)
    {
        for(auto &mod : _modules)
        {
            cmd << mod->dll_file(build_folder()).string();
        }
    }
    
    // Custom flags //
    for(auto &flag : _flags)
    {
        cmd << flag;
    }

    // Common flags //
    handle_common_flags(cmd, settings);
    
    // Module names //
    std::string modules;
    for(auto &mod : _modules)
    {
        modules += "\\\"" + mod->name() + "\\\", ";
    }
    modules.erase(modules.end() - 2, modules.end());

    cmd << ("-D DIBAPP_MODULE_NAMES=\"" + modules + "\"");

    // Add output flag //
    cmd << "-o" << (output / "main").string();
    cmd.execute();

    // Build resources batch if requested //
    if(settings.use_batch)
    {
        res::ResourceBatch::make_from_directory(resources_folder(), build_folder() / ".dibbatch");
    }
}

BuildSettings project::default_build_settings()
{
    return {
        .added_flags = {},
        .dib_path = "/mnt/c/raylib/dib",
        .raylib_path = "/mnt/c/raylib",
        .static_link = false,
        .use_batch = false
    };
}