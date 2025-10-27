#ifndef __DIB_ENV_H
#define __DIB_ENV_H

#include <filesystem>

namespace dib::env
{
    const char *const *argv();
    int argc();

    void initialize(int argc, const char* const *argv);

    std::filesystem::path executable_path();
    std::filesystem::path executable_directory_path();
}

#endif