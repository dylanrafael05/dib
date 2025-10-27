// g++ test/json.cpp src/util/io.manip.cpp src/json/writer.cpp src/json/reader.cpp -Wall -Wextra -std=c++20 -g -o test/json -I ./include
#include "dib/json.h"
#include "dib/debug.h"

#include <iostream>
#include <iomanip>
#include <sstream>

using namespace dib;

/*
std::ostream &operator<<(std::ostream &o, const json::Any &val)
{
    if(auto i = val.as_int())
    {
        return o << *i;
    }
    else if(auto f = val.as_float())
    {
        return o << *f;
    }
    else if(auto s = val.as_string())
    {
        return o << '"' << *s << '"';
    }
    else if(val.as_null())
    {
        return o << "null";
    }
    else if(auto arr = val.as_array())
    {
        o << '[';
        for(auto it = arr->begin(); it != arr->end();)
        {
            o << *it;
            if(++it != arr->end())
                o << ", ";
        }
        o << ']';
    }
    else if(auto obj = val.as_object())
    {
        o << '{';
        for(auto it = obj->begin(); it != obj->end();)
        {
            o << "'" << it->first << "': " << it->second;
            if(++it != obj->end())
                o << ", ";
        }
        o << '}';
    }

    return o;
}
*/

int main()
{
    // const char *text = "{\"flags\": [\"-Wall\", \"-Wextra\"]}";

    // std::stringstream text_stream(text);
    // json::Any value;
    // json::read(text_stream, value);

    // std::cout << value << std::endl;

    json::JsonWriter sjsn(std::cout, false);

    std::vector<int> x = {1, 2, 3};
    std::unordered_map<std::string, std::string> y = {
        {"x", "x"},
        {"a", "b"}
    };
    
    try
    {
        sjsn.write_start_object()
            .write_kvp("type", ".txt")
            .write_key("values")
            .write_start_array()
            .write(json::null)
            .write(10)
            .write("hello!")
            .write(x)
            .write(10.4)
            .write_end_array()
            .write_key("!")
            .write(true)
            .write_end_object();
    }
    catch(const json::JsonException &err)
    {
        std::cerr << err.what() << std::endl;
    }

    std::string jval = R"(
        {
            "type": ".txt",
            "values": [1, 2, 3, true, false, null],
            "foobar": {"foo": "hi", "bar": "ho"},
            "ints": [1, 2, 3]
        }
    )";

    std::stringstream jvst(jval);
    json::JsonReader read(jvst);

    std::cout << jval << std::endl;

    try
    {
        std::string _type, _foo, _bar;
        uint8_t _v1, _v2, _v3;
        bool _v4, _v5;
        std::vector<int> _vi;

        read.read_start_object()
            .expect_kvp("type", _type)
            .expect_key("values")
            .read_start_array()
            .read(_v1).read(_v2).read(_v3)
            .read(_v4).read(_v5).read_null()
            .read_end_array()
            .expect_key("foobar")
            .read_start_object()
            .expect_kvp("foo", _foo)
            .expect_kvp("bar", _bar)
            .read_end_object()
            .expect_kvp("ints", _vi)
            .read_end_object()
            .end();

        assert(_type == ".txt");
        assert(_foo == "hi");
        assert(_bar == "ho");
        assert(_v1 == 1);
        assert(_v2 == 2);
        assert(_v3 == 3);
        assert(_v4 == true);
        assert(_v5 == false);

        for(auto i : _vi)
            std::cout << i;
    }
    catch(const json::JsonReadException &err)
    {
        std::cerr << err.what() << std::endl;
    }
}