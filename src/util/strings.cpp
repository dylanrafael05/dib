#include "dib/conststring.h"
#include "dib/strings.h"

#include <cstring>

using namespace dib::strings;

bool dib::strings::operator==(strings::StringLiteral lit, const char *str)
{
    return str != nullptr && strcmp(lit.c_str(), str) == 0;
}

bool dib::strings::operator==(const char *str, strings::StringLiteral lit)
{
    return str != nullptr && strcmp(lit.c_str(), str) == 0;
}

void dib::strings::split(
    std::string_view haystack, 
    std::string_view needle, 
    dib::structures::Vector<std::string_view> &results)
{
    results.clear();

    while(true)
    {
        auto index = haystack.find(needle);

        if(index == std::string::npos)
            break;

        results.push_back(haystack.substr(0, index));
        haystack = haystack.substr(index + 1);
    }

    results.push_back(haystack);
}