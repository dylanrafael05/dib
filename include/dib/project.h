#ifndef __DIBPROJECT_H
#define __DIBPROJECT_H

#include <unordered_map>
#include <vector>
#include <filesystem>
#include <string>
#include <span>
#include <memory>

#include "conststring.h"

namespace dib::project
{
    namespace detail
    {
        struct ModuleComparator;
    }
    
    class CommandBuilder
    {
        std::string _text;

    public:
        CommandBuilder(const std::string_view &base)
            : _text(base)
        {}

        CommandBuilder &operator<<(const std::string_view &text);
        void execute() const;

        const std::string &text() const {return _text;}
    };

    struct BuildSettings
    {
        std::vector<std::string> added_flags;
        std::filesystem::path dib_path;
        std::filesystem::path raylib_path;
        bool static_link;
        bool use_batch;
    };

    class Project;

    class Module
    {
        Project *_project;
        std::vector<Module*> _dependencies;
        std::string _name;
        std::string _identifier;
        std::filesystem::path _dir;
        int _status = 0;
        int _sorting_order = 0;
        
    public:
        Module(Project &project, const std::filesystem::path &dir);

        std::filesystem::path source_folder() const;
        std::filesystem::path config_file() const;
        std::filesystem::path dll_file(const std::filesystem::path &build, bool is_static = false) const;
        void load_configuration();

        const std::string &name() const {return _name;}
        const std::string &identifier() const {return _identifier;}
        std::span<const Module *const> dependencies() const {return _dependencies;}
        
        void build(const BuildSettings &settings, const std::filesystem::path &output) const;

        friend struct detail::ModuleComparator;
    };

    namespace detail
    {
        struct ModuleComparator
        {
            bool operator()(const std::unique_ptr<Module> &lhs, const std::unique_ptr<Module> &rhs) const;
        };
    }

    class Project
    {
        std::unordered_map<std::string, Module*> _by_name;
        std::vector<std::unique_ptr<Module>> _modules;
        std::filesystem::path _dir;
        std::vector<std::string> _flags;

    public:
        std::filesystem::path config_file() const;
        std::filesystem::path resources_folder() const;
        std::filesystem::path scripts_folder() const;
        std::filesystem::path build_folder() const;
        void load(const std::filesystem::path &dir);

        const Module &module(const std::string &name) const {return *(_by_name.at(name));}
        const Module &module(const std::string_view &name) const {return *(_by_name.at(std::string(name)));}
        const Module &module(const dib::strings::string_literal &name) const {return *(_by_name.at(std::string(name)));}
        
        std::span<const std::unique_ptr<Module>> modules() const {return _modules;}

        void build(const BuildSettings &settings, const std::filesystem::path &output) const;

        friend class Module;
    };

    BuildSettings default_build_settings();
}

#endif