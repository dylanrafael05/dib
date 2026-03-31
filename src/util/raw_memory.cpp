#include "dib/raw_memory.h"
#include "dib/raw_memory_utils.h"
#include "dib/types.h"
#include "dib/debug.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

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
void str::ErasedStack::push(refl::Type type, void *contents)
{
    buffer.insert(buffer.end(), (uint8_t *)contents, (uint8_t *)contents + type.packed_size());
    buffer.insert(buffer.end(), (uint8_t *)&type, (uint8_t *)&type + sizeof(type));
}

ErasedStack::Value str::ErasedStack::top()
{
    if(buffer.empty())
        RUNTIME_ERROR("Attempt to get the top of an empty ErasedStack");

    auto type = *(refl::Type *)(&buffer.back() - sizeof(refl::Type) + 1);

    return Value
    {
        .type = type,
        .pointer = &buffer.back() - sizeof(refl::Type) - type.packed_size() + 1
    };
}

void str::ErasedStack::pop_nondestructive()
{
    auto value = top();
    buffer.erase(buffer.begin() + ((uint8_t *) value.pointer - &buffer.front()), buffer.end());
}

void str::ErasedStack::pop()
{
    auto value = top();
    value.type.destruct(value.pointer);
    buffer.erase(buffer.begin() + ((uint8_t *) value.pointer - &buffer.front()), buffer.end());
}