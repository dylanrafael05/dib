#ifndef __DIB_HOTRELOAD_H
#define __DIB_HOTRELOAD_H

#include "dib/project.h"

#include <filesystem>
#include <vector>
#include <map>
#include <string_view>

namespace dib::hotreload
{
    class DLL
    {
        void *handle = nullptr;
        std::filesystem::path dll_path;

    public:
        DLL(const std::filesystem::path &path) 
            : dll_path(path)
        {}
        DLL() {}

        void load();
        void unload();
        void *sym(std::string_view str) const;

        const std::filesystem::path &path() const {return dll_path;}
    };

    class DynamicContext
    {
        using timepoint = std::filesystem::file_time_type;

        struct Entry
        {
            const dib::project::Module *mod = nullptr;

            DLL dll;
            bool is_temp = false;

            Entry() {}
            Entry(const dib::project::Module *mod, const DLL &dll, bool is_temp);
        };

        timepoint last_update;
        std::map<std::string_view, Entry> entries;

    public:
        void add(const dib::project::Module *mod);
        void load_all();
        void unload_all();

        void reload(std::string_view module);
        std::vector<std::string_view> poll_for_reload();
    }; 
}

#endif