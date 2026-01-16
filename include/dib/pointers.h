#pragma once

#include <memory>

#include "dib/debug.h"

namespace dib
{
    template<class T>
    class BorrowedPtr
    {
        T *_ptr;

    public:
        BorrowedPtr() : _ptr(nullptr) {}
        BorrowedPtr(T *ptr) : _ptr(ptr) {}
        BorrowedPtr(const std::unique_ptr<T> &ptr) : _ptr(ptr.get()) {}

        operator BorrowedPtr<const T>()
        {
            return { _ptr };
        }

        bool has_value() const { return _ptr; }
        T *get() const { return _ptr; }

        T &operator*() { if(!_ptr) RUNTIME_ERROR("Null pointer dereference"); return *_ptr; }
        const T &operator*() const { if(!_ptr) RUNTIME_ERROR("Null pointer dereference"); return *_ptr; }

        T *operator->() { if(!_ptr) RUNTIME_ERROR("Null pointer dereference"); return _ptr; }
        const T *operator->() const { if(!_ptr) RUNTIME_ERROR("Null pointer dereference"); return _ptr; }

        auto operator==(const BorrowedPtr<T> &other) const { return _ptr == other._ptr; }
        auto operator<=>(const BorrowedPtr<T> &other) const { return _ptr <=> other._ptr; }
    };
}

namespace std
{
    template<class T> struct hash<dib::BorrowedPtr<T>>
    {
        size_t operator()(const dib::BorrowedPtr<T> &ptr) const
        {
            return std::hash<T *>{}(ptr.get());
        }
    };
}