#pragma once

#include <string_view>

#include "dib/vector.h"

namespace dib::strings
{
    void split(
        std::string_view haystack, 
        std::string_view needle, 
        dib::structures::Vector<std::string_view> &results);
}