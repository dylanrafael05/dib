#include "dib/debug.h"
#include "dib/json.h"

#include <variant>
#include <stack>
#include <iostream>
#include <ctype.h>

using namespace dib::json;
using namespace std::string_view_literals;

// VALUE IMPLEMENTATION //
using backing_t = std::variant<Int, Bool, Float, String, Null, Object, Array>;

void detail::AnyDeleter::operator()(void *value) const
{
    delete (backing_t*)value;
} 

template<class T>
std::unique_ptr<void, detail::AnyDeleter> make_backing(T &&value)
{
    return {(void*)new backing_t(std::forward<T>(value)), detail::AnyDeleter{}};
}

const backing_t &get_backing(const std::unique_ptr<void, detail::AnyDeleter> &ptr)
{
    return *(const backing_t*)ptr.get();
}

Any::Any() : ptr(make_backing(null)) {}

Any::Any(Int value) : ptr(make_backing(value)) {}
Any::Any(Bool value) : ptr(make_backing(value)) {}
Any::Any(Float value) : ptr(make_backing(value)) {}
Any::Any(const String &value) : ptr(make_backing(value)) {}
Any::Any(String &&value) : ptr(make_backing(std::move(value))) {}
Any::Any(Null value) : ptr(make_backing(value)) {}
Any::Any(Object &&value) : ptr(make_backing(std::move(value))) {}
Any::Any(Array &&value) : ptr(make_backing(std::move(value))) {}

const Int *Any::as_int() const
{
    if(get_backing(ptr).index() == 0)
        return &std::get<Int>(get_backing(ptr));

    return nullptr;
}

const Bool *Any::as_bool() const
{
    if(get_backing(ptr).index() == 1)
        return &std::get<Bool>(get_backing(ptr));

    return nullptr;
}

const Float *Any::as_float() const
{
    if(get_backing(ptr).index() == 2)
        return &std::get<Float>(get_backing(ptr));

    return nullptr;
}

const String *Any::as_string() const
{
    if(get_backing(ptr).index() == 3)
        return &std::get<String>(get_backing(ptr));

    return nullptr;
}

const Null *Any::as_null() const
{
    if(get_backing(ptr).index() == 4)
        return &std::get<Null>(get_backing(ptr));

    return nullptr;
}

const Object *Any::as_object() const
{
    if(get_backing(ptr).index() == 5)
        return &std::get<Object>(get_backing(ptr));

    return nullptr;
}

const Array *Any::as_array() const
{
    if(get_backing(ptr).index() == 6)
        return &std::get<Array>(get_backing(ptr));

    return nullptr;
}

Any &Object::operator[](std::string &&str)
{
    return values[std::move(str)];
}
Any &Object::operator[](const std::string &str)
{
    return values[str];
}
const Any &Object::operator[](const std::string_view &str) const
{
    return values.find(std::string(str))->second;
}

size_t Object::size() const
{
    return values.size();
}

bool Object::has(const std::string_view &str) const
{
    return values.find(std::string(str)) != values.end();
}

const Any *Object::get_or_null(const std::string_view &str) const
{
    auto it = values.find(std::string(str));

    if(it == values.end()) return nullptr;
    return &it->second;
}

Any &Array::operator[](size_t index)
{
    return values[index];
}
const Any &Array::operator[](size_t index) const
{
    return values[index];
}

void Array::push(Any &&value)
{
    values.push_back(std::move(value));
}

size_t Array::size() const
{
    return values.size();
}

// JsonReader api //
void true_skip_ws(std::istream &str)
{
    while(str && (str.peek() == '\t' || str.peek() == ' ' || str.peek() == '\n' || str.peek() == '\r'))
    {
        str.get();
    }
}

JsonReader::Collection::Collection()
    : is_object(false)
{}

JsonReader::Collection::Collection(bool obj)
    : is_object(obj)
{}

JsonReader::JsonReader()
    : _stream(nullptr), _collections()
{}

JsonReader::JsonReader(std::istream &stream)
    : _stream(&stream), _collections()
{
    true_skip_ws(stream);
}

void JsonReader::end_value()
{
    if(_needs_key)
    {
        RUNTIME_ERROR("Json read exception: expected key to be read, not value");
    }

    if(!*_stream)
    {
        RUNTIME_ERROR("Json read exception: unexpected value");
    }

    true_skip_ws(*_stream);
    if(_collections.empty()) return;

    if(_stream->peek() == ',')
    {
        _stream->get();
        true_skip_ws(*_stream);
        
        if(_collections.top().is_object)
        {
            _needs_key = true;
        }
    }
    else if(_stream->peek() != '}' && _stream->peek() != ']')
    {
        RUNTIME_ERROR("Json read exception: unexpected value");
    }
}

bool JsonReader::at_quote() {return _stream->peek() == '"';}
bool JsonReader::at_digit() {return isdigit(_stream->peek());}
bool JsonReader::at_null() {return _stream->peek() == 'n';}
bool JsonReader::at_open_array() {return _stream->peek() == '[';}
bool JsonReader::at_close_array() {return _stream->peek() == ']';}
bool JsonReader::at_open_object() {return _stream->peek() == '{';}
bool JsonReader::at_close_object() {return _stream->peek() == '}';}

JsonReader &JsonReader::read_null()
{
    char str[5] = {0};
    _stream->read(str, 4);

    if(!*_stream || str != "null"sv)
        RUNTIME_ERROR("Json read exception: expected null");

    end_value();

    return *this;
}

template<class T>
void read_int(std::istream &is, T &value)
{
    uintmax_t max_val = 0;
    bool negative = false;

    if constexpr (std::is_signed_v<T>)
    {
        if (is.peek() == '-')
        {
            negative = true;
            is.get();
        }
    }

    while(is && isdigit(is.peek()))
    {
        max_val *= 10;
        max_val += (uintmax_t)(is.get() - '0');

        if(max_val > (uintmax_t)std::numeric_limits<T>::max())
        {
            RUNTIME_ERROR("Json read exception: integer out of range for requested type");
        }
    }

    value = (T)max_val;
    if(negative)
    {
        value *= -1;
    }
}

template<class T>
void read_float(std::istream &is, T &value)
{
    value = 0;
    bool negative = false;

    float divisor = 1;
    bool past_point = false;

    if(is.peek() == '-')
    {
        negative = true;
        is.get();
    }

    while(is && (isdigit(is.peek()) || (!past_point && is.peek() == '.')))
    {
        if(is.peek() == '.')
        {
            past_point = true;
            is.get();
        }
        else if(past_point)
        {
            divisor *= 10;
            value += (int)(is.get() - '0') / divisor;
        }
        else 
        {
            value *= 10;
            value += (int)(is.get() - '0');
        }
    }

    if(negative) value *= -1;
}

#define impl_int {read_int(*_stream, value); end_value(); return *this;}
#define impl_float {read_float(*_stream, value); end_value(); return *this;}

JsonReader &JsonReader::read(uint8_t &value) impl_int
JsonReader &JsonReader::read(uint16_t &value) impl_int
JsonReader &JsonReader::read(uint32_t &value) impl_int
JsonReader &JsonReader::read(uint64_t &value) impl_int
JsonReader &JsonReader::read(int8_t &value) impl_int
JsonReader &JsonReader::read(int16_t &value) impl_int
JsonReader &JsonReader::read(int32_t &value) impl_int
JsonReader &JsonReader::read(int64_t &value) impl_int

JsonReader &JsonReader::read(float &value) impl_float
JsonReader &JsonReader::read(double &value) impl_float

JsonReader &JsonReader::read(bool &value)
{
    char str[5] = {0};
    _stream->read(str, 4);

    if(str == "true"sv) value = true;
    else if(str == "fals"sv && _stream->get() == 'e') value = false;
    else RUNTIME_ERROR("Json read exception: expected boolean value");

    end_value();

    return *this;
}

void parse_str(std::istream &is, auto l)
{
    if(is.peek() != '"') RUNTIME_ERROR("Json read exception: Expected start of string");
    is.get();

    while(is && is.peek() != '\"')
    {
        if(is.peek() == '\\')
        {
            is.get();
            char esc = is.peek();

            if(esc == '\\') l('\\');
            else if(esc == '"') l('"');
            else RUNTIME_ERROR("Json read exception: unknown escape sequence");
        }
        else 
        {
            l(is.peek());
        }

        is.get();
    }

    if(is.peek() != '\"')
    {
        RUNTIME_ERROR("Json read exception: expected end of string");
    }

    is.get();
}

JsonReader &JsonReader::read(std::string &value)
{
    value = "";

    parse_str(*_stream, [&](char c) {value += c;});
    end_value();

    return *this;
}

JsonReader &JsonReader::read_start_array()
{
    if(!at_open_array()) RUNTIME_ERROR("Json read exception: expected start of array");

    _collections.emplace(false);
    _stream->get();
    true_skip_ws(*_stream);

    return *this;
}

JsonReader &JsonReader::read_end_array()
{
    if(!at_close_array()) RUNTIME_ERROR("Json read exception: expected end of array");

    if(_collections.empty() || _collections.top().is_object)
    {
        RUNTIME_ERROR("Json read exception: end of array requested when at object or empty");
    }

    _collections.pop();
    _stream->get();
    
    end_value();

    return *this;
}

JsonReader &JsonReader::read_start_object()
{
    if(!at_open_object()) RUNTIME_ERROR("Json read exception: expected start of object");

    _collections.emplace(true);
    _stream->get();
    true_skip_ws(*_stream);

    _needs_key = true;

    return *this;
}

JsonReader &JsonReader::read_end_object()
{
    if(!at_close_object()) RUNTIME_ERROR("Json read exception: expected end of object");

    if(_collections.empty() || !_collections.top().is_object)
    {
        RUNTIME_ERROR("Json read exception: end of object requested when at array or empty");
    }

    _collections.pop();
    _stream->get();
    
    end_value();

    return *this;
}

void read_colon(std::istream &is)
{
    true_skip_ws(is);
    if(is.get() != ':') RUNTIME_ERROR("Json read exception: expected colon after key name");

    true_skip_ws(is);
}

JsonReader &JsonReader::read_key(std::string &value)
{
    if(!_needs_key) RUNTIME_ERROR("Json read exception: expected to read a value, not a key");

    value = "";
    parse_str(*_stream, [&](char c) {value += c;});
    read_colon(*_stream);

    _needs_key = false;

    return *this;
}

JsonReader &JsonReader::expect_key(std::string_view key)
{
    if(!_needs_key) RUNTIME_ERROR("Json read exception: expected to read a value, not a key");

    size_t i = 0;
    parse_str(*_stream, [&](char c)
    {
        if(i >= key.size() || key[i] != c) RUNTIME_ERROR("Json read exception: Expected key to match");
        i++;
    });
    read_colon(*_stream);
    
    _needs_key = false;

    return *this;
}

JsonReader &JsonReader::read(Array &value)
{
    read_start_array();

    while(!at_close_array())
    {
        Any _value;
        read(_value);

        value.push(std::move(_value));
    }

    return read_end_array();
}

JsonReader &JsonReader::read(Object &value)
{
    read_start_object();

    while(!at_close_object())
    {
        std::string _key; Any _value;
        read_key(_key);
        read(_value);

        value[_key] = std::move(_value);
    }

    return read_end_object();
}

JsonReader &JsonReader::read(Any &value)
{
    if(at_open_array()) 
    {
        Array arr;
        read(arr);
        value = std::move(arr);
    }
    else if(at_open_object())
    {
        Object arr;
        read(arr);
        value = std::move(arr);
    }
    else if(at_quote())
    {
        std::string arr;
        read(arr);
        value = std::move(arr);
    }
    else if(_stream->peek() == 'n')
    {
        read_null();
        value = json::null;
    }
    else if(_stream->peek() == 't' || _stream->peek() == 'f')
    {
        bool arr;
        read(arr);
        value = arr;
    }
    else if(isdigit(_stream->peek()))
    {
        try 
        {
            Int i;
            read(i);
            value = i;
        }
        catch(const debug::RuntimeError &) // TODO: rework this! catching RuntimeError is dangerous!
        {
            Float f;
            read(f);
            value = f;
        }
    }
    else 
    {
        RUNTIME_ERROR("Json read exception: unknown value kind");
    }

    return *this;
}

void JsonReader::end()
{
    if(_stream->good())
        RUNTIME_ERROR("Json read exception: expected end of json input");
}