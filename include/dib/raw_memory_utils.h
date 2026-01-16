#pragma once

#include <stddef.h>
#include <stdint.h>

#include "dib/types.h"

namespace dib::mem
{
    /// Swap size bytes of the contents of the provided memory locations.
    /// @param first The first location to swap from.
    /// @param second The second location to swap from. 
    /// @param size The amount of bytes to swap.
    void memswap(void *first, void *second, size_t size);

    /// Dereference the provided pointer as though it were a pointer to T.
    template<class T, class P>
    T &read_as(P *buffer) noexcept
    {
        return *reinterpret_cast<T *>(buffer);
    }

    /// Dereference the provided pointer as though it were a pointer to T.
    template<class T, class P>
    const T &read_as(const P *buffer) noexcept
    {
        return *reinterpret_cast<const T *>(buffer);
    }

    /// Add a number of bytes in offset to a void pointer.
    inline void *add_bytes(void *ptr, ptrdiff_t offset) noexcept
    {
        return (void *)((uint8_t *)ptr + offset);
    }

    template<class T, class K> requires (sizeof(K) == sizeof(T) && alignof(K) == alignof(T))
        auto &reference_cast(K &in) noexcept
    {
        return *(reinterpret_cast<types::CopyConst<K, T>*>(std::addressof(in)));
    }

    template<class T>
    T &mutable_cast(const T &in) noexcept
    {
        return *(const_cast<T *>(std::addressof(in)));
    }
}