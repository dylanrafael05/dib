/// g++ test/stack.cpp -I ./include -I ./src -std=c++20 -Wall -Wextra -o test/stack

#include "dib/raw_memory.h"
#include "dib/debug.h"

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <assert.h>

using namespace dib;
using namespace dib::structures;

struct large
{
    char data[1024];
};

int main()
{
    ErasedStack stack;

    DEBUG_ASSERT(stack.size_bytes() == 0);

    using ull = unsigned long long;
    using u8 = unsigned long long; // uint8_t;

    // Standard insertion
    stack.push(10ull);
    DEBUG_ASSERT(stack.size_bytes() > sizeof(ull));

    DEBUG_ASSERT(stack.top().size == sizeof(ull));
    DEBUG_ASSERT(stack.top_as<ull>() == 10ull);
    
    // Small-sized insertion
    stack.push((u8)45);
    DEBUG_ASSERT(stack.top().size == sizeof(u8));
    DEBUG_ASSERT(stack.top_as<u8>() == 45);
    
    // Large-sized insertion
    stack.push(large{});
    DEBUG_ASSERT(stack.top().size == sizeof(large));
    stack.pop();
    
    // Removal
    stack.pop();
    DEBUG_ASSERT(stack.size_bytes() != 0);
    
    // Permenance
    DEBUG_ASSERT(stack.top().size == sizeof(ull));
    DEBUG_ASSERT(stack.top_as<ull>() == 10ull);

    // Removal and retreival
    DEBUG_ASSERT(stack.pop_as<ull>() == 10ull);
    DEBUG_ASSERT(stack.size_bytes() == 0);
}