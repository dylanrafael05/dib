#ifndef __DEBUG_H
#define __DEBUG_H

#include <assert.h>
#include <iostream>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <source_location>

namespace dib::debug
{
    template<class T>
    auto log(T &&value, const char *value_name, std::source_location loc = std::source_location::current()) -> decltype(value)
    {
        std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                  << value_name << " = " << value << std::endl;
        
        return std::forward<T>(value);
    }

    inline void log_buffer(uint8_t *buffer, size_t size)
    {
        std::cout << "[[ BUFFER(size = " << size << ") ]]" << std::endl;
        for(auto it = buffer; it != buffer + size; it++)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)*it << std::dec << ' ';
        }
        std::cout << std::endl;
    }

    inline void make_assertion(bool assertion, const char *value_name, std::source_location loc = std::source_location::current())
    {
        if(!assertion)
        {
            std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                      << "Assertion " << value_name << " failed!" << std::endl;
            
            std::abort();
        }
        else 
        {
            std::cout << loc.file_name() << ':' << loc.line() << ':' << loc.column() << " (in " << loc.function_name() << "); " 
                      << "Assertion " << value_name << " passed!" << std::endl;
        }
    }

    inline void do_not_optimize()
    {
        char dummy = '\0';
        volatile char *dummy_ptr = &dummy;
        *dummy_ptr = '\0';
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

#ifndef NDEBUG
    #define DEBUG_LOG(...) ::dib::debug::log(__VA_ARGS__, #__VA_ARGS__)
    #define DEBUG_ASSERT(...) ::dib::debug::make_assertion(__VA_ARGS__, #__VA_ARGS__)
#else
    #define DEBUG_LOG(...) __VA_ARGS__
    #define DEBUG_ASSERT(...)
#endif

#endif