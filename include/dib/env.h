#pragma once

#include <filesystem>

namespace dib::env
{
    /// Get the argv provided to main
    const char *const *argv();
    /// Get the argc provided to main
    int argc();

    /// Set the argc and argv variables provided from main
    void initialize(int argc, const char* const *argv);

    /// Get the path of the currently executing program
    std::filesystem::path executable_path();
    /// Get the path of the directory containing the currently executing program
    std::filesystem::path executable_directory_path();
}