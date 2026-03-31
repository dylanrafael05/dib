#pragma once

#include <exception>
#include <format>
#include <iostream>
#include <source_location>
#include <string_view>

#include "dib/preprocess.h"

namespace dib::debug
{
    namespace detail
    {
        void *generate_stacktrace();
        void free_stacktrace(void *);
    }

    /// An exception thrown by an erroneous operation at runtime.
    class RuntimeError
    {
        void *_stacktrace;
        std::string _str;

    public:
        RuntimeError(void *trace, std::string &&str)
            : _stacktrace(trace), _str(MOVE(str))
        {}

        ~RuntimeError()
        {
            detail::free_stacktrace(_stacktrace);
        }

        void write_to_stream(std::ostream &ostream) const;
    };

    /// Log a message
    void log(std::source_location loc, std::string_view str);

    template<class... Args>
    void logf(std::source_location loc, std::format_string<Args...> str, Args &&...args)
    {
        log(loc, std::format(str, FORWARD(args)...));
    }

    /// Make an assertion; throwing RuntimeError on failure
    void make_assertion(bool assertion, std::string_view value_name);

    /// Throw a runtime error
    template<class ...Args>
    [[noreturn]] inline constexpr void runtime_error(
        std::format_string<Args...> message, Args &&...args)
    {
        throw RuntimeError(detail::generate_stacktrace(), std::format(message, FORWARD(args)...));
    }
}

/// Helper macros for logging and assertion
#define LOG(...) ::dib::debug::log(::std::source_location::current(), __VA_ARGS__)
#define LOGF(...) ::dib::debug::logf(::std::source_location::current(), __VA_ARGS__)
#define ASSERT(...) ::dib::debug::make_assertion(__VA_ARGS__, #__VA_ARGS__)
#define RUNTIME_ERROR(...) ::dib::debug::runtime_error(__VA_ARGS__)

#ifndef NDEBUG
    #define DEBUG_LOG(...) LOG(__VA_ARGS__)
    #define DEBUG_LOGF(...) LOGF(__VA_ARGS__)
    #define DEBUG_ASSERT(...) ASSERT(__VA_ARGS__)
#else
    #define DEBUG_LOG(...)
    #define DEBUG_LOGF(...)
    #define DEBUG_ASSERT(...)
#endif