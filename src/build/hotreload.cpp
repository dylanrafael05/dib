#include "dib/hotreload.h"
#include "dib/project.h"
#include "dib/env.h"
#include "dib/io/os.h"

#include <chrono>
#include <iostream>

using namespace dib::hotreload;
using namespace dib::project;
using namespace dib;

namespace fs = std::filesystem;

#if DIB_OS_LINUX
#include <dlfcn.h>

void DLL::load()
{
    handle = dlopen(dll_path.c_str(), RTLD_LAZY);

    if(!handle)
    {
        std::cerr << dlerror() << std::endl;
        std::abort();
    }
}

void DLL::unload()
{
    int err = dlclose(handle);
    handle = NULL;
    
    if(err)
    {
        std::cerr << dlerror() << std::endl;
        std::abort();
    }
}

void *DLL::sym(std::string_view str) const
{
    return dlsym(handle, str.data());
}

#elif DIB_OS_WIN
#include <Windows.h>

void DLL::load()
{
    auto mod_name = dll_path.string();
    handle = LoadLibraryA(mod_name.c_str());

    if(!handle)
    {
        std::cerr << "An error occured while loading dll " << mod_name << std::endl;
        std::abort();
    }
}

void DLL::unload()
{
    if(FreeLibrary((HMODULE)handle))
    {
        std::cerr << "An error occured while unloading dll " << dll_path << std::endl;
        std::abort();
    }
}

void *DLL::sym(std::string_view str) const
{
    return (void*)GetProcAddress((HMODULE)handle, str.data());
}

#else
    #error "Unsupported platform for module loading."
#endif


DynamicContext::Entry::Entry(const dib::project::Module *mod, const DLL &dll, bool is_temp)
    : mod(mod), dll(dll), is_temp(is_temp)
{}

void DynamicContext::add(const Module *mod)
{
    entries[mod->name()] = Entry{
        mod,
        {mod->dll_file(env::executable_directory_path())},
        false
    };
}

void DynamicContext::load_all()
{
    for(auto &[_, entry] : entries)
    {
        entry.dll.load();
    }

    last_update = (timepoint)std::chrono::system_clock::now().time_since_epoch();
}

void DynamicContext::unload_all()
{
    for(auto &[_, entry] : entries)
    {
        entry.dll.unload();
        if(entry.is_temp)
        {
            fs::remove(entry.dll.path());
        }
    }
}

void DynamicContext::reload(std::string_view module)
{
    auto &entry = entries[module];
    
    entry.dll.unload();
    if(entry.is_temp)
    {
        fs::remove(entry.dll.path());
    }

    // TODO: use proper build settings here instead of the default.

    auto newfile = entry.mod->dll_file(env::executable_directory_path());
    auto updtstr = std::to_string((long long)last_update.time_since_epoch().count());
    newfile.replace_filename(updtstr + "." + newfile.filename().string());

    entry.mod->build(project::default_build_settings(), newfile);
    
    entry.dll = {newfile};
    entry.dll.load();
}

std::vector<std::string_view> DynamicContext::poll_for_reload()
{
    std::vector<std::string_view> to_reload;

    for(auto &[modname, entry] : entries)
    {
        for(auto &f : fs::recursive_directory_iterator{entry.mod->source_folder()})
        {
            if(f.last_write_time() > last_update)
            {
                to_reload.push_back(modname);
                break;
            }
        }
    }

    last_update = (timepoint)std::chrono::system_clock::now().time_since_epoch();
    
    return to_reload;
}