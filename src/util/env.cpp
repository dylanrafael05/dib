#include "dib/env.h"
#include "dib/debug.h"

namespace fs = std::filesystem;

static bool initialized;
static int argc;
static const char *const *argv;
static fs::path initial_path;

void dib::env::initialize(int argc, const char *const *argv)
{
    ASSERT(!initialized);

    ::argc = argc;
    ::argv = argv;

    initial_path = fs::current_path();

    initialized = true;
}

int dib::env::argc() {ASSERT(initialized); return ::argc;}
const char *const *dib::env::argv() {ASSERT(initialized); return ::argv;}

#ifdef _WIN32
    #include <windows.h>
#endif

fs::path dib::env::executable_path()
{
    ASSERT(initialized);

    #ifdef _WIN32

        char path[1024] = {0};
        GetModuleFileName(NULL, path, 1024);
        return fs::path(std::string(path));

    #else

        auto callpath = fs::path(::argv[0]);
        if(callpath.is_absolute())
        {
            return callpath;
        }
        
        return fs::canonical(initial_path / callpath);

    #endif
}

fs::path dib::env::executable_directory_path()
{
    return executable_path().remove_filename();
}