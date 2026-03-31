#pragma once

// TODO: split into mem_utils.h, erased.h, and temp_alloc.h

#include <concepts>
#include <stddef.h>
#include <mutex>
#include <vector>
#include <utility>

#include "dib/reflect.h"
#include "dib/types.h"
#include "dib/raw_memory_utils.h"
#include "dib/debug.h"
#include "dib/algorithm.h"

namespace dib::mem
{
    /// <summary>
    /// A statically-allocated buffer which allocates objects temporarily,
    /// such that allocations are live at least until another call is made
    /// to /*buffer*/.allocate().
    /// </summary>
    /// <typeparam name="N">The size in bytes to allocate for the whole buffer.</typeparam>
    template<size_t N = 4098>
    struct TemporaryBuffer
    {
        TemporaryBuffer() : _offset(0) {}

        void *allocate(size_t size)
        {
            if (size > N) throw std::bad_alloc{};

            _offset += size;
            if (_offset > N) _offset = 0;

            return _buffer[_offset];
        }

    private:
        size_t _offset;
        char _buffer[N];
    };

    /// @brief A statically allocated, stack-based arena of a configurable size.
    /// Should the
    /// @tparam N The size of the arena, defaults to 1000 bytes.
    template<size_t N = 1000>
    struct Scratchpad
    {
        /// @brief The largest allocation which can occur in this arena.
        static constexpr size_t MAX_ALLOCATION_SIZE = (1 << 15) - 1 < N ? (1 << 15) - 1 : N;

        Scratchpad()
            : offset(0), mutex()
        {}

        // LAYOUT //
        // [data] [h][data] [h][data] [H]

        /// @brief The header attached to each allocation.
        struct AllocationHeader
        {
            uint16_t size : 15;
            bool in_use : 1;
        };

        char buffer[N];
        size_t offset = 0;

        mutable std::mutex mutex;

        #if 0
            void debug()
            {
                constexpr auto endl = "\n\t";

                std::cout << std::endl << "BEGIN DEBUG" << endl;
                std::cout << "[[ Scratchpad Allocator Debug ]]" << endl
                        << "   current offset = " << offset << endl;
                auto it = (char*)buffer + offset;

                auto header = (AllocationHeader*)it - 1;

                while(it > buffer)
                {
                    std::cout << "|| Allocation Block ||" << endl
                            << "   size = " << header->size << endl
                            << "   in_use = " << header->in_use << endl;

                    it -= header->size + sizeof(AllocationHeader);
                    header = (AllocationHeader*)it - 1;
                }

                std::cout << "\nEND DEBUG" << std::endl;
            }
        #endif
    };

    template<class T, size_t N>
    struct ScratchpadAllocator
    {
    private:
        Scratchpad<N> *scratchpad;
        using Header = typename Scratchpad<N>::AllocationHeader;

    public:
        using value_type = T;

        template<class X>
        constexpr operator ScratchpadAllocator<X, N>() const
        {
            return {*scratchpad};
        }

        template<class X>
        struct rebind
        {
            using other = ScratchpadAllocator<X, N>;
        };

        char *buffer() const { return scratchpad->buffer; }

        template<class X>
        bool operator==(const ScratchpadAllocator<X, N> &other) const { return buffer() == other.buffer(); }
        template<class X>
        bool operator!=(const ScratchpadAllocator<X, N> &other) const { return buffer() != other.buffer(); }

        ScratchpadAllocator(Scratchpad<N> &scratchpad)
            : scratchpad(&scratchpad)
        {}

        constexpr size_t max_size() const
        {
            return Scratchpad<N>::MAX_ALLOCATION_SIZE;
        }

        T *allocate(size_t n) // OUTLINE (? is this possible)
        {
            size_t size = n * sizeof(T);

            if(size > max_size())
            {
                return (T*)std::malloc(size);
            }

            std::lock_guard lock {scratchpad->mutex};

            auto off = scratchpad->offset;
            auto end = off + size;

            if(end + sizeof(Header) > N)
            {
                return (T*)std::malloc(size);
            }

            auto alloc = reinterpret_cast<T*>(&scratchpad->buffer[0] + off);
            auto header = reinterpret_cast<Header*>(&scratchpad->buffer[0] + end);
            header->size = size;
            header->in_use = true;

            scratchpad->offset = end + sizeof(Header);

            return alloc;
        }

        void deallocate(T *pointer, size_t n) // OUTLINE (? is this possible)
        {
            auto buf = (size_t)(char*)scratchpad->buffer;
            auto ptr = (size_t)(char*)pointer;

            if(ptr >= buf + N || ptr < buf)
            {
                std::free(pointer);
                return;
            }

            std::lock_guard lock { scratchpad->mutex };

            auto header = reinterpret_cast<Header*>(pointer + n);
            header->in_use = false;

            auto calculate_top_header = [&]
            {
                return (Header*)&scratchpad->buffer[scratchpad->offset] - 1;
            };

            auto top_header = calculate_top_header();

            while(scratchpad->offset != 0 && !top_header->in_use)
            {
                scratchpad->offset -= top_header->size + sizeof(Header);
                top_header = calculate_top_header();
            }
        }
    };

    template<class T, size_t N>
    ScratchpadAllocator<T, N> make_allocator(Scratchpad<N> &pad)
    {
        return {pad};
    }
    
    /// @brief Stores a value of the provided type whose destructor
    /// will not be called automatically.
    /// @tparam T The type to store.
    template<class T>
    class Forgotten
    {
        union { T val; char _; };

    public:
        template<class... Args>
        constexpr Forgotten(Args &&...args)
            : val(FORWARD(args)...)
        {}

        constexpr Forgotten(const Forgotten &other)
            : val(other.val)
        {}
        constexpr Forgotten(Forgotten &&other)
            : val(MOVE(other.val))
        {}

        constexpr ~Forgotten() {}

        /// @brief Retrieve the held value.
        constexpr T &value() {return val;}
        
        /// @brief Retrieve the held value.
        constexpr const T &value() const {return val;}

        /// @brief Destruct inplace the held value.
        /// Usage of 'value' beyond this point is UB.
        constexpr void drop()
        {
            val.~T();
        }
    };

    /// @brief "Prevent" the destructor of the provided value from being run. The destructor is still called, but on a moved-from
    /// object, and thus should not destruct the actual values held by the original object.
    /// @param value The value to forget.
    template<std::move_constructible T>
    void forget(T &&value)
    {
        using T_ = std::remove_cvref_t<T>;
        Forgotten<T_> _(MOVE(value));
    }
    
    namespace detail
    {
        static inline char zst_storage = 0;
    }

    /// Create a pointer to a valid, dereferenceable memory address
    /// for a zero-sized-type (which in C++ is one byte large).
    template<types::IsZST T>
    T *pointer_to_zst()
    {
        return (T*) &detail::zst_storage;
    }
}

namespace dib::structures
{
    // Manual destruction //
    using ErasedPtr = void *;

    class ErasedVec
    {
        size_t element_size = 0;
        refl::Type descriptor;

        uint8_t *ptr = nullptr;
        size_t _size = 0;
        size_t _capacity = 0;

        void grow();

    public:
        using iterator = dib::algorithm::BasicRandomAccessIterator<ErasedVec>;
        using const_iterator = dib::algorithm::BasicRandomAccessIterator<const ErasedVec>;

        // TODO: element_size can be folded into the descriptor! yay!
        ErasedVec(size_t element_size, refl::Type descriptor)
            : element_size(element_size), descriptor(descriptor), ptr(nullptr), _size(0), _capacity(0)
        {}
        ErasedVec()
            : ErasedVec(0, {})
        {}

        size_t get_element_size() const {return element_size;}
        refl::Type get_descriptor() const {return descriptor;}

        void alloc_back();
        void push_back(ErasedPtr value);
        void remove_by_swap(size_t index);
        ErasedPtr take_by_swap(size_t index);
        void pop_back();

        ErasedPtr inplace_take(size_t index);
        void inplace_destruct(size_t index);
        
        void uninitialized_assign(size_t index, ErasedPtr value);

        ErasedPtr pointer(size_t index) const
        {
            return (ErasedPtr)(ptr + index * element_size);
        }

        size_t size() const {return _size;}
        size_t capacity() const {return _capacity;}

        void deallocate() const 
        {
            if(ptr) delete[] ptr;
        }

        template<class T, class... Args>
        void emplace_back(Args &&...args)
        {
            dib::mem::Forgotten<T> value(std::forward<Args>(args)...);
            push_back((ErasedPtr)&value);
        }
        
        template<class T, class... Args>
        void uninitialized_assign(size_t index, Args &&...args)
        {
            dib::mem::Forgotten<T> value(std::forward<Args>(args)...);
            uninitialized_assign(index, (ErasedPtr)&value);
        }

        template<class T>
        T &get(size_t index) 
        {
            return *(T*)pointer(index);
        }
        
        template<class T>
        const T &get(size_t index) const
        {
            return *(T*)pointer(index);
        }

        dib::refl::AnyRef get(size_t index)
        {
            return { descriptor, pointer(index) };
        }
        const dib::refl::AnyRef get(size_t index) const
        {
            return { descriptor, pointer(index) };
        }

        iterator begin() { return {this, 0}; }
        iterator end() { return begin() + size(); }
        const_iterator begin() const { return { this, 0 }; }
        const_iterator cbegin() const { return begin(); }
        const_iterator end() const { return cbegin() + size(); }
        const_iterator cend() const { return end(); }

        template<class T>
        static ErasedVec create()
        {
            return {
                types::packed_sizeof<T>,
                refl::typeof<T>
            };
        }
    };

    /// @brief A type-erased structure which stores elements of non-homogenous type in sequence.
    /// Values stored and returned from this structure must be destructed by the caller.
    /// Memory within this structure is never reclaimed once used.
    class ErasedStack
    {
        // TODO: once dib::structures::Vector is mature enough, use it instead
        // TODO: currently, the implementation of this type relies on std::vector to play nice
        //       with popped values (it will not clean them up, but rather will clobber them later)
        // TODO: refactor API to 'pop' into external locations rather than returning a pointer to
        //       potentially invalid memory
        std::vector<uint8_t> buffer;

    public:
        /// @brief A value stored inside an instance of a stack
        struct Value
        {
            refl::Type type;
            void *pointer;
        };

        /// Append a value to this stack by memcpy-ing from the contents.
        /// No destruction takes place internally, so contents should either be trivial or
        /// wrapped in a memory::Forgotten and forgotten.
        /// @param size The size, in bytes, of the element being added.
        /// @param contents A pointer to the element.
        void push(refl::Type type, void *contents);

        /// Retrieve an element from this stack without removing it.
        Value top();

        /// Remove the topmost element, calling its destructor in place.
        void pop();
        
        /// Remove the topmost element without calling its desructor.
        void pop_nondestructive();
        
        /// Templated and reference-friendly version of push
        template<class T>
        void push(const T &contents)
        {
            push(refl::typeof<T>, (void*) &contents);
        }

        /// Templated and reference-friendly version of top
        template<class T>
        T &top_as()
        {
            auto value = top();
            if (value.type != refl::typeof<T>)
            {
                RUNTIME_ERROR("Reading incorrect type from erased stack. Attempted to get as {}, but it is {}",
                    refl::typeof<T>.name(), value.type.name());
            }

            return dib::mem::read_as<T>(value.pointer);
        }

        /// @brief Get the size, in bytes, that this stack is actively using.
        size_t size_bytes() const {return buffer.size();}
    };

    // Raw values //
    class ErasedSingleton
    {
        void *data;
        refl::Type type;

    public:
        ErasedSingleton()
        {
            data = nullptr;
        }

        template<class T>
        ErasedSingleton(T &&reference)
        {
            data = new char[sizeof(std::remove_cvref_t<T>)];
            new(data) T(FORWARD(reference));

            type = refl::typeof<T>;
        }

        template<class T>
        T &get() const 
        { 
            if(refl::typeof<T> != type)
                RUNTIME_ERROR("Attempt to cast a singleton to a type it is not.");
            
            return *(T*)data;
        }

        ErasedSingleton(ErasedSingleton &&other) noexcept
        {
            data = std::exchange(other.data, nullptr);
            type = other.type;
        }

        ErasedSingleton &operator=(ErasedSingleton &&other) noexcept
        {
            data = std::exchange(other.data, nullptr);
            type = other.type;

            return *this;
        }

        ~ErasedSingleton() 
        {
            if (data)
            {
                type.destruct(data);
                delete[](char *)data;
            }
        }
    };
}