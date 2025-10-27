#ifndef __DIB_RAW_MEMORY_UTILS
#define __DIB_RAW_MEMORY_UTILS

#include <stddef.h>
#include <stdint.h>

namespace dib::mem
{
    namespace detail
    {
        template<class From, class To>
        struct CopyConstType { using type = std::remove_const_t<To>; };
        template<class From, class To>
        struct CopyConstType<const From, To> { using type = std::add_const_t<To>; };

        template<class From, class To>
        using copy_const_t = CopyConstType<From, To>::type;
    }

    /// @brief Swap size bytes of the contents of the provided memory locations.
    /// @param first The first location to swap from.
    /// @param second The second location to swap from. 
    /// @param size The amount of bytes to swap.
    void memswap(void *first, void *second, size_t size);

    /// @brief Dereference the provided pointer as though it were a pointer to T.
    /// @tparam T The type to dereference as.
    template<class T, class P>
    T &read_as(P *buffer) noexcept
    {
        return *reinterpret_cast<T *>(buffer);
    }

    /// @brief Dereference the provided pointer as though it were a pointer to T.
    /// @tparam T The type to dereference as.
    template<class T, class P>
    const T &read_as(const P *buffer) noexcept
    {
        return *reinterpret_cast<const T *>(buffer);
    }

    /// @brief Add a number of bytes in offset to a void pointer.
    inline void *add_bytes(void *ptr, ptrdiff_t offset) noexcept
    {
        return (void *)((uint8_t *)ptr + offset);
    }

    template<class T, class K> requires (sizeof(K) == sizeof(T) && alignof(K) == alignof(T))
        detail::copy_const_t<K, T> &reference_cast(K &in) noexcept
    {
        return *(reinterpret_cast<detail::copy_const_t<K, T>*>(std::addressof(in)));
    }

    template<class T>
    T &mutable_cast(const T &in) noexcept
    {
        return *(const_cast<T *>(std::addressof(in)));
    }
}

#endif