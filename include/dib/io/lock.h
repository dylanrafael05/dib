#ifndef __DIBIO_LOCK_H
#define __DIBIO_LOCK_H

#include <filesystem>

namespace dib::io
{
    struct lock_handle;
    
    lock_handle lock(const std::filesystem::path &path);
    void unlock(lock_handle lock);

    struct lock_handle
    {
    private:
        void *ptr;

        lock_handle(void *ptr) : ptr(ptr) {}

    public:
        friend lock_handle dib::io::lock(const std::filesystem::path &path);
        friend void dib::io::unlock(lock_handle lock);
    };

    struct lock_guard
    {
    private:
        lock_handle handle;

    public:
        lock_guard(const std::filesystem::path &path);
        ~lock_guard();
    };
}

#endif