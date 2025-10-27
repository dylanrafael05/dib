#ifndef __DIBAPP_IO_MANIP_H
#define __DIBAPP_IO_MANIP_H

#include <iosfwd>

namespace dib::io
{
    class indent
    {
        char fill;
        size_t tabsize;
        size_t count;

    public:
        indent(size_t count, char fill=' ', size_t tabsize=4);

        friend std::ostream &operator<<(std::ostream &stream, indent in);
    };
    
    std::ostream &operator<<(std::ostream &stream, indent in);
}

#endif