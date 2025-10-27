#include "dib/raw_memory.h"

#include <algorithm>
#include <cstring>
#include <array>
#include <numeric>

using namespace dib::structures;
namespace str = dib::structures;

void dib::mem::memswap(void *first, void *second, size_t size)
{
    auto cfirst  = static_cast<char*>(first);
    auto csecond = static_cast<char*>(second);

    auto efirst = cfirst + size;

    while(cfirst != efirst)
    {
        std::swap(*cfirst, *csecond);

        cfirst++;
        csecond++;
    }
}

#if 0
    #include <iomanip>
    #define DEBUG                                                                                   \
    {                                                                                               \
        std::cout << "ELEMENT_SIZE = " << element_size << std::endl;                                \
        std::cout << "CAPACITY = " << capacity() << std::endl;                                      \
        std::cout << "SIZE = " << size() << std::endl;                                              \
        std::cout << "BUFFER: ";                                                                    \
        auto cnt = 0;                                                                               \
        for(auto it = ptr; it != ptr + capacity() * element_size; it++)                             \
        {                                                                                           \
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)*it << " "; \
            if(++cnt % element_size == 0) std::cout << "| ";                                        \
        }                                                                                           \
        std::cout << std::endl << std::dec;                                                         \
    } 
#else
    #define DEBUG
#endif

// ERASED VEC //
constexpr size_t BASE_CAPACITY = 4;

void str::ErasedVec::grow()
{
    if(element_size == 0)
        return;

    auto new_capacity = std::max(_capacity * 2, BASE_CAPACITY);
    uint8_t *new_ptr = new uint8_t[new_capacity * element_size];

    if(_capacity)
    {
        // TODO: somehow work `relocate_n` into this?
        for (auto p = ptr, n = new_ptr; p < ptr + _size * element_size; p += element_size, n += element_size)
        {
            descriptor.uninitialized_relocate(p, n);
        }

        delete[] ptr;
    }

    ptr = new_ptr;
    _capacity = new_capacity;
}

void str::ErasedVec::alloc_back()
{
    if(element_size == 0)
    {
        _size++;
        return;
    }

    if(_size >= _capacity)
        grow();
    
    _size++;

    DEBUG;
}

void str::ErasedVec::uninitialized_assign(size_t index, ErasedPtr value)
{
    descriptor.uninitialized_relocate(value, ptr + index * element_size);
}

void str::ErasedVec::push_back(ErasedPtr value)
{
    if(element_size == 0)
    {
        _size++;
        return;
    }

    if(_size >= _capacity)
        grow();

    descriptor.uninitialized_relocate(value, ptr + _size * element_size);
    _size++;
}

void str::ErasedVec::remove_by_swap(size_t index)
{
    if(element_size == 0)
    {
        _size++;
        return;
    }

    auto element = ptr + index * element_size;
    _size--;

    descriptor.swap(element, ptr + _size * element_size);
    descriptor.destruct(static_cast<ErasedPtr>(ptr + _size * element_size));
}

ErasedPtr str::ErasedVec::take_by_swap(size_t index)
{
    if(element_size == 0)
        return nullptr;

    auto element = ptr + index * element_size;
    _size--;

    descriptor.swap(ptr + _size * element_size, element);
    return ptr + _size * element_size;
}

ErasedPtr str::ErasedVec::inplace_take(size_t index)
{
    if(element_size == 0)
        return nullptr;

    DEBUG;

    auto element = ptr + index * element_size;
    return element;
}

void str::ErasedVec::inplace_destruct(size_t index)
{
    if(element_size == 0)
        return;

    auto element = ptr + index * element_size;
    descriptor.destruct(element);
}

void str::ErasedVec::pop_back()
{
    inplace_destruct(size() - 1);
    _size--;
}

// TODO: create and implement ErasedVec::erase //

// ERASED STACK //
size_t str::ErasedStack::read_size(size_t &size) const
{
    switch(buffer.back())
    {
        case size_marker::u8: // u8 //
            size = dib::mem::read_as<uint8_t>(&buffer.back() - 1);
            return 1;
        
        case size_marker::u16: // u16 //
            size = dib::mem::read_as<uint16_t>(&buffer.back() - 2);
            return 2;

        case size_marker::u32: // u32 //
            size = dib::mem::read_as<uint32_t>(&buffer.back() - 4);
            return 4;
            
        default: // u64 //
            size = dib::mem::read_as<uint64_t>(&buffer.back() - 8);
            return 8;
    }
}

void str::ErasedStack::push(size_t size, const uint8_t *contents)
{
    buffer.insert(buffer.end(), contents, contents + size);

    if(size < std::numeric_limits<uint8_t>::max())
    {
        buffer.push_back((uint8_t)size);
        buffer.push_back(size_marker::u8);
    }
    else if(size < std::numeric_limits<uint16_t>::max())
    {
        auto u16 = std::bit_cast<std::array<uint8_t, 2>>((uint16_t)size);
        buffer.insert(buffer.end(), u16.begin(), u16.end());
        buffer.push_back(size_marker::u16);
    }
    else if(size < std::numeric_limits<uint32_t>::max())
    {
        auto u32 = std::bit_cast<std::array<uint8_t, 4>>((uint32_t)size);
        buffer.insert(buffer.end(), u32.begin(), u32.end());
        buffer.push_back(size_marker::u32);
    }
    else
    {
        auto u64 = std::bit_cast<std::array<uint8_t, 8>>((uint64_t)size);
        buffer.insert(buffer.end(), u64.begin(), u64.end());
        buffer.push_back(size_marker::u64);
    }
}

ErasedStack::Value str::ErasedStack::top()
{
    size_t size;
    auto off = read_size(size);
    auto ptr = &buffer.back() - off - size;

    return Value
    {
        .size = size,
        .pointer = ptr
    };
}

ErasedStack::Value str::ErasedStack::pop()
{
    auto value = top();

    size_t size;
    auto off = read_size(size);

    buffer.erase(buffer.end() - 1 - off - size, buffer.end());
    
    return value;
}

// Inhomogenious stack //
void str::InhomogeneousStack::push(types::TypeDescriptor desc, void *value)
{
    auto start_idx = buffer.size();
    buffer.resize(buffer.size() + desc.packed_size() + sizeof desc);

    auto dest = buffer.data() + start_idx;
    desc.uninitialized_relocate(value, dest);

    auto desc_dest = buffer.data() + start_idx + desc.packed_size();
    mem::read_as<types::TypeDescriptor>(desc_dest) = desc;

    count++;
}

void *str::InhomogeneousStack::top()
{
    auto end = buffer.data() + buffer.size();
    return end - top_type().packed_size() - sizeof(types::TypeDescriptor);
}

const void *str::InhomogeneousStack::top() const
{
    auto end = buffer.data() + buffer.size();
    return end - top_type().packed_size() - sizeof(types::TypeDescriptor);
}

auto str::InhomogeneousStack::top_type() const -> types::TypeDescriptor
{
    auto end = buffer.data() + buffer.size();
    return mem::read_as<types::TypeDescriptor>(end - sizeof(types::TypeDescriptor));
}

void str::InhomogeneousStack::pop_relocated()
{
    buffer.resize(buffer.size() - top_type().packed_size() - sizeof(types::TypeDescriptor));
    count--;
}

void str::InhomogeneousStack::pop()
{
    top_type().destruct(top());
    pop_relocated();
}

void str::InhomogeneousStack::uninitialized_relocate_top(void *dest)
{
    top_type().uninitialized_relocate(top(), dest);
    pop_relocated();
}