#include "../include/sparse_bitset.h"
#include "../include/raw_memory.h"
#include <iostream>

using namespace dib::structures;

int main()
{
    dib::mem::Scratchpad<1024> scratch;
    auto alloc = dib::mem::make_allocator<int>(scratch);

    SparseBitset::with_alloc<decltype(alloc)> bits{alloc};

    bits.set(0);
    bits.set(10);
    bits.set(11);
    bits.set(128);
    bits.set(180);
    bits.set(64);
    bits.set(300);

    std::cout << "0: " << bits.test(0) << std::endl;
    std::cout << "1: " << bits.test(1) << std::endl;

    for(auto bit : bits)
    {
        std::cout << bit << ", ";
    }
    std::cout << std::endl;
}