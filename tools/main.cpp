#include "dib/resources.h"
#include "dib/project.h"
#include "dib/env.h"

#include <iostream>
#include <string_view>
#include <filesystem>

using namespace std::string_view_literals;
using namespace dib::project;
using namespace dib;

namespace fs = std::filesystem;

constexpr const char *ansi_red = "\x1b[31m";
constexpr const char *ansi_green = "\x1b[32m";
constexpr const char *ansi_bold = "\x1b[1m";
constexpr const char *ansi_lean = "\x1b[3m";
constexpr const char *ansi_clr = "\x1b[0m";

void build()
{
    if(env::argc() < 3)
    {
        std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "incorrect number of arguments!" << std::endl
                  << ansi_lean << "Please specify the project folder to build." << std::endl
                  << std::endl;
        exit(1);
    }

    fs::path proj = env::argv()[2];
    if(!fs::exists(proj))
    {
        std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "could not find project folder '" << proj << "'" << std::endl
                  << std::endl;
        exit(1);
    }

    // TODO: store and parse persistent settings to and from disk
    BuildSettings settings = project::default_build_settings();

    bool forwarding_args = false;
    for(int i = 3; i < env::argc(); i++)
    {
        auto arg = env::argv()[i];

        if(forwarding_args)
        {
            settings.added_flags.push_back(arg);
        }
        else if(arg == "--"sv)
        {
            forwarding_args = true;
        }
        else if(arg == "--static"sv || arg == "-S"sv)
        {
            settings.static_link = true;
        }
        else if(arg == "--batch"sv || arg == "-B"sv)
        {
            settings.use_batch = true;
        }
        else 
        {
            std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "unrecognized argument '" << arg << "'." << std::endl
                      << ansi_lean << "Did you mean to forward this argument to the compiler using '--'?" << std::endl
                      << std::endl;
            exit(1);
        }
    }

    project::Project proj_inst;
    proj_inst.load(proj);

    proj_inst.build(settings, proj_inst.build_folder());
}

void create_mod()
{
    std::cerr << "UNIMPLEMENTED" << std::endl;
    exit(1);
}

void create_project()
{
    std::cerr << "UNIMPLEMENTED" << std::endl;
    exit(1);
}

void make_batch()
{
    if(env::argc() < 4)
    {
        std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "incorrect number of arguments!" << std::endl
                  << ansi_lean << "Please specify the project folder to build as well as the target file path." << std::endl
                  << std::endl;
        exit(1);
    }

    fs::path folder(env::argv()[2]);
    fs::path target(env::argv()[3]);

    dib::res::ResourceBatch::make_from_directory(folder, target);
}

int main(int argc, char **argv)
{
    env::initialize(argc, argv);

    if(argc < 2)
    {
        std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "incorrect number of arguments!" << std::endl
                  << ansi_lean << "Please specify an operation from the following: build, new_mod, new_project" << std::endl
                  << std::endl;
        exit(1);
    }

    if(argv[1] == "build"sv)
    {
        std::cerr << ansi_lean << ansi_bold << "Use of this command is deprecated." << ansi_clr << std::endl;
        build();
    }
    else if(argv[1] == "new_mod"sv)
    {
        create_mod();
    }
    else if(argv[1] == "new_project"sv)
    {
        create_project();
    }
    else if(argv[1] == "make-batch"sv)
    {
        make_batch();
    }
    else 
    {
        std::cout << ansi_red << ansi_bold << "Error: " << ansi_clr << "unknown command " << argv[1] << std::endl
                  << ansi_lean << "Please specify an operation from the following: build, mod" << std::endl
                  << std::endl;
        exit(1);
    }
}