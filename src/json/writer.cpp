#include "dib/json.h"
#include "dib/io/manip.h"
#include <iostream>

using namespace dib;
using namespace dib::json;

JsonWriter::Collection::Collection()
    : after_first_object(false), is_object(false)
{}

JsonWriter::Collection::Collection(bool obj)
    : after_first_object(false), is_object(obj)
{}

JsonWriter::JsonWriter()
    : _stream(nullptr), _collections()
{}

JsonWriter::JsonWriter(std::ostream &stream, bool is_pretty)
    : _stream(&stream), _collections(), _pretty(is_pretty)
{}

void JsonWriter::start_value()
{
    start_value_pre();
    start_value_post();
}

void JsonWriter::start_value_pre()
{
    if(_needs_key)
    {
        RUNTIME_ERROR("Json exception: expected a key for the current json object.");
    }

    if(!_collections.empty() && _collections.top().after_first_object && !_collections.top().is_object)
    {
        *_stream << ", ";
        if(_pretty)
        {
            *_stream << std::endl << io::indent(_indent);
        }
    }
}

void JsonWriter::start_value_post()
{
    if(!_collections.empty())
    {
        _collections.top().after_first_object = true;
        _needs_key = _collections.top().is_object;
    }
}

JsonWriter &JsonWriter::write(Null)
{
    start_value();
    *_stream << "null";

    return *this;
}

#define impl_val { start_value(); *_stream << value; return *this; }

JsonWriter &JsonWriter::write(int8_t value) impl_val
JsonWriter &JsonWriter::write(int16_t value) impl_val
JsonWriter &JsonWriter::write(int32_t value) impl_val
JsonWriter &JsonWriter::write(int64_t value) impl_val
JsonWriter &JsonWriter::write(uint8_t value) impl_val
JsonWriter &JsonWriter::write(uint16_t value) impl_val
JsonWriter &JsonWriter::write(uint32_t value) impl_val
JsonWriter &JsonWriter::write(uint64_t value) impl_val
JsonWriter &JsonWriter::write(float value) impl_val
JsonWriter &JsonWriter::write(double value) impl_val

JsonWriter &JsonWriter::write(bool value)
{
    start_value();
    *_stream << (value ? "true" : "false");

    return *this;
}

JsonWriter &JsonWriter::write(std::string_view value)
{
    start_value();
    *_stream << '"' << value << '"';
    return *this;
}

JsonWriter &JsonWriter::write(const std::string &value)
{
    return write((std::string_view)value);
}

JsonWriter &JsonWriter::write(const char *value)
{
    return write((std::string_view)value);
}


JsonWriter &JsonWriter::write(const Object &object)
{
    write_start_object();
    for(auto &[k, v] : object)
    {
        write_key(k);
        write(v);
    }
    write_end_object();
    return *this;
}

JsonWriter &JsonWriter::write(const Array &array)
{
    write_start_array();
    for(auto &v : array)
    {
        write(v);
    }
    write_end_array();
    return *this;
}

JsonWriter &JsonWriter::write(const Any &any)
{
    if(auto i = any.as_int()) write(*i);
    if(auto i = any.as_bool()) write(*i);
    if(auto i = any.as_float()) write(*i);
    if(auto i = any.as_null()) write(*i);
    if(auto i = any.as_array()) write(*i);
    if(auto i = any.as_object()) write(*i);
    if(auto i = any.as_string()) write((std::string_view)*i);
    return *this;
}

JsonWriter &JsonWriter::write_key(std::string_view value)
{
    if(!_needs_key)
    {
        RUNTIME_ERROR("Json exception: unexpected key.");
    }

    _needs_key = false;
    
    if(_collections.top().after_first_object && _collections.top().is_object)
    {
        *_stream << ", ";
        if(_pretty)
        {
            *_stream << std::endl << io::indent(_indent);
        }
    }

    *_stream << '"' << value << "\": ";
    return *this;
}

JsonWriter &JsonWriter::write_key(const std::string &value)
{
    return write_key((std::string_view)value);
}

JsonWriter &JsonWriter::write_key(const char *value)
{
    return write_key((std::string_view)value);
}

JsonWriter &JsonWriter::write_start_object()
{
    start_value_pre();

    *_stream << '{';
    _collections.emplace(true);
    _indent++;

    if(_pretty)
    {
        *_stream << std::endl << io::indent(_indent);
    }

    _needs_key = true;

    return *this;
}
JsonWriter &JsonWriter::write_end_object()
{
    if(_collections.empty() || !_collections.top().is_object)
    {
        RUNTIME_ERROR("Json exception: cannot end an object which has not yet been created.");
    }
    
    _indent--;

    if(_pretty)
    {
        *_stream << std::endl << io::indent(_indent);
    }

    *_stream << '}';

    _collections.pop();
    start_value_post();

    return *this;
}

JsonWriter &JsonWriter::write_start_array()
{
    // TODO: HANDLE ARRAYS IN OBJECTS!
    start_value_pre();

    *_stream << '[';
    _collections.emplace(false);
    _indent++;

    if(_pretty)
    {
        *_stream << std::endl << io::indent(_indent);
    }

    return *this;
}
JsonWriter &JsonWriter::write_end_array()
{
    if(_collections.empty() || _collections.top().is_object)
    {
        RUNTIME_ERROR("Json exception: cannot end an array which has not yet been created.");
    }

    _indent--;
    
    if(_pretty)
    {
        *_stream << std::endl << io::indent(_indent);
    }

    *_stream << ']';
    
    _collections.pop();
    start_value_post();

    return *this;
}

void JsonWriter::set_pretty(bool pretty)
{
    this->_pretty = pretty;
}

bool JsonWriter::is_pretty() const
{
    return this->_pretty;
}