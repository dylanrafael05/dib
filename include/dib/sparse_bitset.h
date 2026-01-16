#pragma once 

#include <stdint.h>
#include <stddef.h>
#include <array>
#include <memory>

#include "dib/types.h"
#include "dib/functional.h"

// This file contains the definition of a 'sparse bitset', which is
// a structure which stores a bitset in a sparse manner (bits
// are packed into 16bit words which are then stored in a sorted list
// by their 'start index').

namespace dib::structures
{    
    /// An iterator into a sparse bitset
    class SparseBitsetIterator;

    namespace detail
    {
        /// An 'entry' within a sparse bitset
        struct SparseBitsetEntry
        {
            uint16_t index;
            uint16_t bits;

            bool operator==(const SparseBitsetEntry &other) const
            {
                return index == other.index && bits == other.bits;
            }
            bool operator!=(const SparseBitsetEntry &other) const
            {
                return !operator==(other);
            }
        };

        /// A helper type that stores references to allocation functions
        struct EntryAllocatorFuncs
        {
            dib::functional::FunctionRef<SparseBitsetEntry *(size_t)> alloc;
            dib::functional::FunctionRef<void(SparseBitsetEntry *, size_t)> free;
        };
        
        /// The base implementation of a sparse bitset, agnostic of allocator
        class SparseBitset_Impl : public types::HashProvided
        {
        protected:
            // Helper typedefs //
            using Entry = detail::SparseBitsetEntry;
            
            constexpr static size_t STACK_THRESHOLD = 6;

            union 
            {
                Entry *dynamic;
                std::array<Entry, STACK_THRESHOLD> stack;
            };

            size_t count;

            Entry *begin_entries();
            const Entry *begin_entries() const;

            Entry *end_entries() {return begin_entries() + count;}
            const Entry *end_entries() const {return begin_entries() + count;}

            Entry *find_entry(uint16_t index);
            const Entry *find_entry(uint16_t index) const;

            Entry *new_entry(uint16_t index, EntryAllocatorFuncs alloc);
            void remove_entry(Entry *entry, EntryAllocatorFuncs alloc);

            void copy_from(const SparseBitset_Impl &other, EntryAllocatorFuncs alloc);
            void move_from(SparseBitset_Impl &&from);
            void cleanup(EntryAllocatorFuncs alloc);
            
            void set(size_t value, EntryAllocatorFuncs alloc);
            void unset(size_t value, EntryAllocatorFuncs alloc);
            
            SparseBitset_Impl or_with(const SparseBitset_Impl &other, EntryAllocatorFuncs alloc);
            SparseBitset_Impl and_with(const SparseBitset_Impl &other, EntryAllocatorFuncs alloc);

            friend class structures::SparseBitsetIterator;

        public:
            constexpr static size_t MAX_INDEX = (1 << 16) * 16;

            SparseBitset_Impl() : stack(), count(0) {}

            bool test(size_t value) const;
            size_t get_hash() const;

            SparseBitsetIterator begin() const;
            SparseBitsetIterator end() const;

            bool operator==(const SparseBitset_Impl &other) const;
            bool operator!=(const SparseBitset_Impl &other) const {return !operator==(other);}

            bool is_subset_of(const SparseBitset_Impl &other) const;
        };
        
        static_assert(
            dib::types::IsHashProvided<SparseBitset_Impl>, 
            "SparseBitset_Impl should provide hash properly!");
    }

    template<class Allocator = std::allocator<detail::SparseBitsetEntry>>
    class SparseBitset_Alloc final : public detail::SparseBitset_Impl
    {
        // Allocator handling //
        using _EntryAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<Entry>;
        using _EntryAllocTraits = std::allocator_traits<_EntryAllocator>;
        using _EntryAllocFns = detail::EntryAllocatorFuncs;

        _EntryAllocator allocator;

        // Allocator function object generation //
        static _EntryAllocFns _alloc_from(_EntryAllocator &allocator)
        {
            return _EntryAllocFns
            {
                // TODO: this is janky!
                .alloc = decltype(_EntryAllocFns::alloc){
                    [](_EntryAllocator *env, size_t count) 
                    {
                        return _EntryAllocTraits::allocate(*env, count);
                    },
                    &allocator
                },
                
                .free = decltype(_EntryAllocFns::free){
                    [](_EntryAllocator *env, Entry *ptr, size_t count) 
                    {
                        return _EntryAllocTraits::deallocate(*env, ptr, count);
                    },
                    &allocator
                }
            };
        }

        auto _alloc()
        {
            return _alloc_from(allocator);
        }

        // Construct from parts //
        SparseBitset_Alloc(SparseBitset_Alloc &&base, const _EntryAllocator &&alloc)
            : SparseBitset_Impl(std::move(base)), allocator(std::move(alloc))
        {}

    public:
        using allocator_type = Allocator;
        _EntryAllocator &get_allocator() {return allocator;}

        // Exposed constructors //
        SparseBitset_Alloc()
            : SparseBitset_Impl(), allocator({})
        {}

        explicit SparseBitset_Alloc(const Allocator &allocator)
            : SparseBitset_Impl(), allocator((_EntryAllocator)allocator)
        {}

        SparseBitset_Alloc(const SparseBitset_Alloc &other)
        {
            allocator = _EntryAllocTraits::select_on_container_copy_construction(other.allocator);
            copy_from(other, _alloc());
        }
        template<class OAlloc>
        SparseBitset_Alloc(const SparseBitset_Alloc<OAlloc> &other, const Allocator &allocator)
            : allocator((_EntryAllocator)allocator)
        {
            copy_from(other, _alloc());
        }

        SparseBitset_Alloc(SparseBitset_Alloc &&other) noexcept
        {
            allocator = std::move(other.allocator);
            move_from(std::move(other));
        }
        template<class OAlloc>
        SparseBitset_Alloc(SparseBitset_Alloc<OAlloc> &&other, const Allocator &allocator)
            : allocator((_EntryAllocator)allocator)
        {
            allocator = std::move(other.allocator);
            move_from(std::move(other));
        }

        SparseBitset_Alloc &operator=(const SparseBitset_Alloc &other)
        {
            auto alloc = _alloc();
            cleanup(alloc);
            copy_from(other, alloc);
            return *this;
        }
        SparseBitset_Alloc &operator=(SparseBitset_Alloc &&other) noexcept
        {
            cleanup(_alloc());
            move_from(std::move(other));
            return *this;
        }

        ~SparseBitset_Alloc()
        {
            cleanup(_alloc());
        }

        // Delegating methods //
        void set(size_t value)
        {
            auto alloc = _alloc();
            detail::SparseBitset_Impl::set(value, alloc);
        }
        void unset(size_t value)
        {
            auto alloc = _alloc();
            detail::SparseBitset_Impl::unset(value, alloc);
        }

        SparseBitset_Alloc or_with(const SparseBitset_Alloc &other)
        {
            auto oalloc = _EntryAllocTraits::select_on_container_copy_construction(allocator);
            auto alloc = _alloc_from(oalloc);
            return {detail::SparseBitset_Impl::or_with(other, alloc), std::move(oalloc)};
        }
        SparseBitset_Alloc and_with(const SparseBitset_Alloc &other)
        {
            auto oalloc = _EntryAllocTraits::select_on_container_copy_construction(allocator);
            auto alloc = _alloc_from(oalloc);
            return {detail::SparseBitset_Impl::and_with(other, alloc), std::move(oalloc)};
        }

        static SparseBitset_Alloc from_bits(std::initializer_list<size_t> set, const Allocator &alloc = {})
        {
            SparseBitset_Alloc out{alloc};
            
            for(auto bit : set)
            {
                out.set(bit);
            }

            return out;
        }

        template<class OAlloc>
        using with_alloc = SparseBitset_Alloc<OAlloc>;

        using iterator = SparseBitsetIterator;
    };

    using SparseBitset = SparseBitset_Alloc<>;
    
    class SparseBitsetIterator
    {
        using Entry = detail::SparseBitsetEntry;

        const Entry *entry_iter = nullptr;
        const Entry *entry_end = nullptr;
        uint16_t bits_iter = 0;
        uint8_t bit_count = 0;

        friend class detail::SparseBitset_Impl;

        void increment();
        
        SparseBitsetIterator(const Entry *entry, const Entry *end, uint16_t bits)
        {
            entry_iter = entry;
            entry_end = end;
            bits_iter = bits;
            bit_count = 0;
        }

    public:
        SparseBitsetIterator()
        {}

        bool operator==(const SparseBitsetIterator &other) const 
        {
            return entry_iter == other.entry_iter
                && bits_iter == other.bits_iter
                && bit_count == other.bit_count;
        }
        bool operator!=(const SparseBitsetIterator &other) const {return !operator==(other);}

        SparseBitsetIterator &operator++()
        {
            increment();
            return *this;
        }
        SparseBitsetIterator operator++(int)
        {
            SparseBitsetIterator copy = *this;
            this->operator++();
            return copy;
        }

        size_t operator*() const;
    };
}