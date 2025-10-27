#include "dib/sparse_bitset.h"
#include "dib/function_util.h"

#include <algorithm>
#include <iostream>
#include <assert.h>

using namespace dib::structures;
using namespace dib::structures::detail;
namespace det = dib::structures::detail;

// Helper functions //
constexpr static uint16_t get_entry(size_t index)
{
    return (uint16_t)(index >> 4);
}

static uint16_t mask(size_t index)
{
    return (uint16_t)(1 << (index & 0xF));
}

template<class It> 
constexpr static bool valid_iterator(It it, uint16_t entry, It &&end)
{
    return it != end && it->index == entry;
}

#if 0
    
    #include <iomanip>
    void debug_report(SparseBitsetEntry *begin, SparseBitsetEntry *end)
    {
        std::cout << "BEGIN REPORT " << "\n\t";
        while(begin != end)
        {
            std::cout << "[[ Entry ]]" << "\n\t"
                    << "Index = " << begin->index << "\n\t"
                    << "Bits = 0x" << std::hex << std::setw(4) << std::setfill('0') << begin->bits << std::dec << "\n\n";

            begin++;
        }
    }

#endif

// Implementations //
void det::SparseBitset_Impl::copy_from(const SparseBitset_Impl &from, EntryAllocatorFuncs alloc)
{
    count = from.count;

    if(from.count > STACK_THRESHOLD)
    {
        // dynamic copy //
        dynamic = alloc.alloc(count);

        for(size_t i = 0; i < count; i++)
            dynamic[i] = from.dynamic[i];
    }
    else 
    {
        // preallocated copy //
        stack = from.stack;
    }
}

void det::SparseBitset_Impl::move_from(SparseBitset_Impl &&from)
{
    count = from.count;
    
    if(from.count > STACK_THRESHOLD)
    {
        // dynamic move //  
        dynamic = from.dynamic;
        from.stack = {};
    }
    else 
    {
        // preallocated move //
        stack = from.stack;
    }
}

void det::SparseBitset_Impl::cleanup(EntryAllocatorFuncs alloc)
{
    if(count > STACK_THRESHOLD)
    {
        alloc.free(dynamic, count);
    }
}

#define begin_impl                                        \
    {                                                     \
        if(count > STACK_THRESHOLD) return dynamic;       \
        else                        return &*stack.begin(); \
    }

const SparseBitsetEntry *det::SparseBitset_Impl::begin_entries() const begin_impl
      SparseBitsetEntry *det::SparseBitset_Impl::begin_entries()       begin_impl

#undef begin_impl

#define find_entry_impl                     \
    {                                       \
        auto begin = this->begin_entries(); \
        auto end = this->end_entries();     \
                                            \
        return std::lower_bound             \
        (                                   \
            /* Range */                     \
            begin, end,                     \
                                            \
            /* Value */                     \
            entry,                          \
                                            \
            /* Comparator */                \
            [](const Entry &e, uint16_t id) \
            {                               \
                return e.index < id;        \
            }                               \
        );                                  \
    } 

const SparseBitsetEntry *det::SparseBitset_Impl::find_entry(uint16_t entry) const find_entry_impl
      SparseBitsetEntry *det::SparseBitset_Impl::find_entry(uint16_t entry)       find_entry_impl

#undef find_entry_impl

SparseBitsetEntry *det::SparseBitset_Impl::new_entry(uint16_t entry, EntryAllocatorFuncs alloc)
{
    Entry *loc;

    if(count < STACK_THRESHOLD)
    {
        // preallocated add //
        loc = find_entry(entry);
        if(valid_iterator(loc, entry, end_entries()))
            return loc;

        for(auto it = end_entries(); it > loc; it--)
        {
            std::swap(*(it - 1), *it);
        }

        loc->index = entry;
        loc->bits = 0;
    }
    else 
    {
        // dynamic add //
        auto destination = alloc.alloc(count + 1);
        loc = find_entry(entry);
        if(valid_iterator(loc, entry, end_entries()))
            return loc;

        size_t i = 0;
        for(auto it = begin_entries(); it != end_entries(); it++, i++)
        {
            if(it == loc)
            {
                destination[i].index = entry;
                destination[i].bits = 0;

                i++;
            }
            
            destination[i] = *it;
        }

        if(loc == end_entries())
        {
            destination[count].index = entry;
            destination[count].bits = 0;
        }

        loc = destination + (loc - begin_entries());

        if(count > STACK_THRESHOLD)
        {
            alloc.free(dynamic, count);
        }

        dynamic = destination;
    }

    count++;
    
    return loc;
}

void det::SparseBitset_Impl::remove_entry(Entry *entry, EntryAllocatorFuncs alloc)
{
    if(count > STACK_THRESHOLD + 1)
    {
        // dynamic removal //
        auto new_dynamic = alloc.alloc(count - 1);
        auto dynamic = this->dynamic;
        auto it = dynamic;

        this->dynamic = new_dynamic;

        for(size_t i = 0; i < count - 1; i++, it++)
        {
            if(it == entry)
            {
                it++;
            }

            new_dynamic[i] = *it;
        }

        delete[] dynamic;
    }
    else if(count == STACK_THRESHOLD + 1)
    {
        // dynamic to preallocated //
        auto dynamic = this->dynamic;
        auto it = dynamic;

        stack = {};
        for(size_t i = 0; i < count - 1; i++, it++)
        {
            if(it == entry)
            {
                it++;
            }
            
            stack[i] = *it;
        }

        delete[] dynamic;
    }
    else 
    {
        // preallocated removal //
        for(auto it = entry; it != end_entries(); it++)
        {
            std::swap(*it, *(it + 1));
        }
    }
    
    count--;
}

bool det::SparseBitset_Impl::test(size_t index) const 
{
    auto entry = get_entry(index);
    auto loc = find_entry(entry);

    return valid_iterator(loc, entry, end_entries()) 
        && (loc->bits & mask(index)) != 0;
}

void det::SparseBitset_Impl::set(size_t index, EntryAllocatorFuncs alloc) 
{
    auto entry = get_entry(index);
    auto loc = new_entry(entry, alloc);

    loc->bits |= mask(index);
}

void det::SparseBitset_Impl::unset(size_t index, EntryAllocatorFuncs alloc) 
{
    auto entry = get_entry(index);
    auto loc = find_entry(entry);

    if(!valid_iterator(loc, entry, end_entries()))
        return;

    loc->bits &= ~mask(index);

    if(loc->bits == 0)
        remove_entry(loc, alloc);
}

bool det::SparseBitset_Impl::operator==(const SparseBitset_Impl &other) const
{
    // Simple case; equal bitsets must have the same number of entries //
    if(count != other.count) return false;

    // Complex case; check that there are no mismatches in the entries //
    auto [a, b] = std::mismatch(begin_entries(), end_entries(), other.begin_entries());
    return a == end_entries();
}

bool det::SparseBitset_Impl::is_subset_of(const SparseBitset_Impl &other) const
{
    auto it_a = begin_entries();
    auto it_b = other.begin_entries();

    while(it_a != end_entries())
    {
        while(it_b->index < it_a->index)
        {
            it_b++;

            // Other bitset does not have the final entries of this bitset //
            if(it_b == other.end_entries())
                return false;
        }

        // Other bitset 'skipped' an entry of this bitset //
        if(it_b->index != it_a->index)
            return false;

        // One or more bits from this set are not set in the other //
        if((it_a->bits & it_b->bits) != it_a->bits)
            return false;

        it_a++;
    }

    return true;
}

// Set operations //
det::SparseBitset_Impl det::SparseBitset_Impl::or_with(const SparseBitset_Impl &other, EntryAllocatorFuncs alloc)
{
    auto union_counter = std::set_union(
        begin_entries(), end_entries(), other.begin_entries(), other.end_entries(), 
        dib::functional::CountingIterator{},
        [](const Entry &a, const Entry &b) {return a.index < b.index;});
    
    auto union_count = union_counter.count;

    SparseBitset_Impl output;

    output.count = union_count;
    if(output.count > STACK_THRESHOLD)
    {
        output.dynamic = alloc.alloc(output.count);
    }

    auto it_a = begin_entries();
    auto it_b = other.begin_entries();
    auto it_out = output.begin_entries();

    while(it_a != end_entries() && it_b != other.end_entries())
    {
        if(it_a->index == it_b->index)
        {
            it_out->index = it_a->index;
            it_out->bits = it_a->bits | it_b->bits;

            it_a++;
            it_b++;
        }
        else if(it_a->index > it_b->index)
        {
            *it_out = *it_a;
            it_a++;
        }
        else 
        {
            *it_out = *it_b;
            it_b++;
        }
    }

    if(it_a != end_entries())
    {
        std::copy(it_a, end_entries(), it_out);
    }
    else
    {
        std::copy(it_b, other.end_entries(), it_out);
    }

    return output;
}

det::SparseBitset_Impl det::SparseBitset_Impl::and_with(const SparseBitset_Impl &other, EntryAllocatorFuncs alloc)
{
    auto union_counter = std::set_intersection(
        begin_entries(), end_entries(), other.begin_entries(), other.end_entries(), 
        dib::functional::CountingIterator{},
        [](const Entry &a, const Entry &b) {return a.index < b.index;});
    
    auto union_count = union_counter.count;

    SparseBitset_Impl output;

    output.count = union_count;
    if(output.count > STACK_THRESHOLD)
    {
        output.dynamic = alloc.alloc(output.count);
    }

    auto it_a = begin_entries();
    auto it_b = other.begin_entries();
    auto it_out = output.begin_entries();
    size_t count = 0;

    while(it_a != end_entries() && it_b != other.end_entries())
    {
        if(it_a->index == it_b->index && (it_a->bits & it_b->bits) != 0)
        {
            it_out->index = it_a->index;
            it_out->bits = it_a->bits & it_b->bits;
            count++;

            it_a++;
            it_b++;
        }
        else if(it_a->index > it_b->index)
        {
            it_a++;
        }
        else 
        {
            it_b++;
        }
    }

    output.count = count;

    if(count != union_count)
    {
        SparseBitset_Impl copy;
        copy.copy_from(output, alloc);

        output.count = union_count;
        output.cleanup(alloc);

        return copy;    
    }

    return output;
}

SparseBitsetIterator det::SparseBitset_Impl::begin() const
{
    if(count == 0) return end();
    return {begin_entries(), end_entries(), begin_entries()->bits};
}

SparseBitsetIterator det::SparseBitset_Impl::end() const
{
    return {end_entries(), end_entries(), 0};
}

size_t det::SparseBitset_Impl::hash() const
{
    std::hash<size_t> hasher;
    size_t result = 0;
    size_t i = 1;

    for(auto element : *this)
    {
        result ^= hasher(element + i) * i;

        i += 5;
        i = ~i;
        i = std::max(i, (size_t)1);
    }

    return result;
}

// Iteration //
void dib::structures::SparseBitsetIterator::increment()
{
    bits_iter >>= 1;
    bit_count++;

    if(bits_iter == 0)
    {
        bit_count = 0;

        if(++entry_iter == entry_end)
        {
            return;
        }

        bits_iter = entry_iter->bits;
    }

    while((bits_iter & 1) == 0)
    {
        bits_iter >>= 1;
        bit_count++;

        assert(bit_count <= 16);
    }
}

size_t dib::structures::SparseBitsetIterator::operator*() const
{
    return (size_t)(entry_iter->index << 4) | (size_t)bit_count;
}