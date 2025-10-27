#include "../include/raw_memory.h"
#include "../include/debug.h"
#include <vector>

int main_alloc()
{
    dib::mem::Scratchpad<1024> scratch;
    auto alloc = dib::mem::make_allocator<int>(scratch);

    std::vector<int, decltype(alloc)> test(alloc);

    for(int i = 0; i < 400; i++)
    {
        test.push_back(i);
    }
}

int main()
{
    dib::structures::ErasedStack stack;

    auto K = 10;
    stack.push(sizeof K, (uint8_t*) &K);
    assert(K == stack.pop_as<decltype(K)>());

    
    auto X = 3.1415926;
    stack.push(sizeof X, (uint8_t*) &X);
    
    char Y[] = "HELLO WORLD!";
    stack.push(sizeof Y, (uint8_t*) &Y);
    
    assert(std::string(Y) == std::string(stack.pop_as<decltype(Y)>()));
    assert(X == stack.pop_as<decltype(X)>());
}