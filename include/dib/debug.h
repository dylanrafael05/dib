#pragma once

#include <format>
#include <iostream>
#include <stacktrace>
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <source_location>

#include "dib/preprocess.h"

namespace dib::debug
{
    /// An exception thrown by an erroneous operation at runtime.
    /// TODO: refactor to hold a std::stacktrace object which can be accessed later,
    ///       to help give better error messages
    class RuntimeError : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /// Log a message
    template<class T>
    void log(T &&value, std::source_location loc = std::source_location::current())
    {
        std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                  << value << std::endl;
    }

    /// Log a value and return it
    template<class T>
    auto log_value(T &&value, const char *value_name, std::source_location loc = std::source_location::current()) -> decltype(value)
    {
        std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                  << value_name << " = " << value << std::endl;
        
        return FORWARD(value);
    }

    /// Log a buffer
    inline void log_buffer(uint8_t *buffer, size_t size)
    {
        std::cout << "[[ BUFFER(size = " << size << ") ]]" << std::endl;
        for(auto it = buffer; it != buffer + size; it++)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)*it << std::dec << ' ';
        }
        std::cout << std::endl;
    }

    /// Make an assertion; throwing RuntimeError on failure
    inline void make_assertion(bool assertion, const char *value_name, std::source_location loc = std::source_location::current())
    {
        if(!assertion)
        {
            std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                      << "Assertion " << value_name << " failed!" << std::endl;
            
            throw RuntimeError(std::format("Assertion {} failed!", value_name));
        }
    }

    /// Throw a runtime error
    inline void runtime_error(auto &&message, std::stacktrace stack)
    {
        throw RuntimeError(std::format("ERROR!\n{}\nStack trace:\n{}", FORWARD(message), stack));
    }
}

namespace dib
{
    [[noreturn]] inline void unreachable()
    {
        std::cerr << "Unreachable code being executed." << std::endl;
        std::abort();
    }
}

/// Helper macros for logging and assertion
#define LOG(...) ::dib::debug::log(__VA_ARGS__)
#define ASSERT(...) ::dib::debug::make_assertion(__VA_ARGS__, #__VA_ARGS__)
#define RUNTIME_ERROR(...) ::dib::debug::runtime_error(__VA_ARGS__, std::stacktrace::current())

#ifndef NDEBUG
    #define DEBUG_LOG(...) ::dib::debug::log(__VA_ARGS__)
    #define DEBUG_LOG_VAL(...) ::dib::debug::log_value(__VA_ARGS__, #__VA_ARGS__)
    #define DEBUG_ASSERT(...) ::dib::debug::make_assertion(__VA_ARGS__, #__VA_ARGS__)
#else
    #define DEBUG_LOG(...)
    #define DEBUG_LOG_VAL(...) __VA_ARGS__
    #define DEBUG_ASSERT(...)
#endif