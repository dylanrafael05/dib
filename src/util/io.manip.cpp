#include "dib/io/manip.h"
#include <iostream>

using namespace dib::io;

indent::indent(size_t count, char fill, size_t tabsize)
    : fill(fill), tabsize(tabsize), count(count)
{}

std::ostream &dib::io::operator<<(std::ostream &stream, indent in)
{
    for(size_t i = 0; i < in.count * in.tabsize; i++)
        stream << in.fill;

    return stream;
}