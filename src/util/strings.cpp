#include "dib/conststring.h"
#include <cstring>

using namespace dib::strings;

bool dib::strings::operator==(string_literal lit, const char *str)
{
    return str != nullptr && strcmp(lit.c_str(), str) == 0;
}

bool dib::strings::operator==(const char *str, string_literal lit)
{
    return str != nullptr && strcmp(lit.c_str(), str) == 0;
}