#include "dib/debug.h"
#include "dib/iou.h"
#include "dib/record.h"
#include "dib/types.h"

#include <source_location>
#include <unordered_set>

#include "external/backward.hpp"

//! THIS CRASHES !//
// backward::SignalHandling sh;

struct [[=dib::provides_hash]] ErrorSource : dib::types::TriviallyRelocatable
{
    size_t line;
    std::string filename;

    size_t get_hash() const { return dib::get_hash(line, filename); }
    bool operator==(const ErrorSource &other) const { return line == other.line && filename == other.filename; }
};

namespace dib::debug
{
    static dib::IOU<std::unordered_set<ErrorSource>> seen_error_sources;

    namespace detail
    {
        void *generate_stacktrace()
        {
            auto st = new backward::StackTrace;
            st->load_here();
            // TODO: figure out why this breaks.
            return st;
        }

        void free_stacktrace(void *ptr)
        {
            delete (backward::StackTrace *)ptr;
        }
    }
}

void dib::debug::RuntimeError::write_to_stream(std::ostream &out) const
{
    backward::TraceResolver traceResolver;
    backward::Printer printer;

    auto &stacktrace = *(backward::StackTrace*)_stacktrace;

    auto source_entry = traceResolver.resolve(stacktrace[0]);
    auto source = ErrorSource 
    { 
        .line = source_entry.source.line, 
        .filename = source_entry.source.filename, 
    };

    out << "[[ERROR]] " << _str << std::endl;

    if(seen_error_sources.value().contains(source))
    {
        out << "[[Stack Trace -- Abridged]] from " << source.filename << ":" << source.line << std::endl;
        return;
    }

    for(size_t i = 0; i < stacktrace.size(); i++)
    {
        auto trace = traceResolver.resolve(stacktrace[i]);

        out << "#" << (stacktrace.size() - i - 1) << "\t" 
            << trace.source.filename << ':' << trace.source.line << ':' << trace.source.col
            << " in " << trace.source.function << std::endl;
    }

    seen_error_sources.value().insert(source);
}

void dib::debug::log(std::source_location loc, std::string_view str)
{
    std::cout << "[[INFO]] " << str << std::endl 
              << "         from " << loc.file_name() << ":" << loc.line() << ":" << loc.column() << std::endl;
}

void dib::debug::make_assertion(bool assertion, std::string_view value_name)
{
    if(!assertion)
    {
        RUNTIME_ERROR("Assertion failed! {}", value_name);
    }
}