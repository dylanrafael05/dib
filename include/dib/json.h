#pragma once

#include <memory>
#include <vector>
#include <stack>
#include <unordered_map>
#include <variant>
#include <functional>
#include <cstdint>
#include <string>
#include <string_view>
#include <iosfwd>
#include <ranges>
#include <concepts>

#include "dib/collections.h"
#include "dib/metautils.h"
#include "dib/types.h"
#include "dib/option.h"
#include "dib/vector.h"
#include "dib/json.attr.h"
#include "dib/math/vec.h"
#include "dib/literals.h"
#include "raylib.h"
#include "meta"

namespace dib::json
{
    /// Define the types that can be stored within a json.

    using Int = int64_t;
    using Bool = bool;
    using Float = float;
    using String = std::string;
    using Null = decltype(nullptr);
    
    struct Object;
    struct Array;

    /// Define a json::Any type which holds any value that can be stored
    /// in a json file. To do this, we use a unique_ptr to void with a custom,
    /// externally defined deleter. This avoids complaints about declaration
    /// ordering from the compiler.

    namespace detail
    {
        struct AnyDeleter
        {
            void operator()(void *value) const;
        };
    }
    
    struct Any
    {
    private:
        std::unique_ptr<void, detail::AnyDeleter> ptr;

    public:
        Any();
        Any(Int);
        Any(Float);
        Any(Bool);
        Any(const String &);
        Any(String &&);
        Any(Null);
        Any(Object &&);
        Any(Array &&);

        const Int *as_int() const;
        const Bool *as_bool() const;
        const Float *as_float() const;
        const String *as_string() const;
        const Null *as_null() const;
        const Object *as_object() const;
        const Array *as_array() const;
    };

    constexpr static Null null = nullptr;

    /// A json object; a mapping of string keys to arbitrarily typed
    /// json values.
    struct Object
    {
    private:
        using map_t = std::unordered_map<String, Any>;
        map_t values;

    public:
        Any &operator[](std::string &&str);
        Any &operator[](const std::string &str);
        const Any &operator[](const std::string_view &str) const;
        
        const Any *get_or_null(const std::string_view &str) const;
        bool has(const std::string_view &str) const;

        size_t size() const;
        
        map_t::iterator begin() {return values.begin();}
        map_t::const_iterator begin() const {return values.begin();}
        map_t::iterator end() {return values.end();}
        map_t::const_iterator end() const {return values.end();}
    };

    /// A json object; a vector of arbitrarily typed json values.
    struct Array
    {
    private:
        std::vector<Any> values;
        
    public:
        Any &operator[](size_t index);
        const Any &operator[](size_t index) const;
        
        size_t size() const;

        void push(Any &&value);

        std::vector<Any>::iterator begin() {return values.begin();}
        std::vector<Any>::const_iterator begin() const {return values.begin();}
        std::vector<Any>::iterator end() {return values.end();}
        std::vector<Any>::const_iterator end() const {return values.end();}
    };

    /// Forward declare json reader and writer to help in definition
    /// of JsonInterface.
    class JsonWriter;
    class JsonReader;

    /// The interface used to seiralize custom types to and from json.
    template<class T>
    struct JsonInterface
    {
        static void write(JsonWriter &writer, const T &value);
        static void read(JsonReader &reader, T &value);

        using IsDefault = void;
    };

    /// A marker type used to denote that this type declares its own
    /// json interface. This entails either the definition of a member
    /// void handle_json(Json &), or the definition of two members:
    /// void write_to_json(JsonWriter &) const, void read_from_json(JsonReader &)
    struct ProvidedJsonInterface : types::Marker<ProvidedJsonInterface> {};

    /// A concept which checks if a type has a custom implementation of
    /// JsonInterface.
    template<class T>
    concept HasCustomInterface = 
        !requires 
        {
            typename JsonInterface<T>::IsDefault;
        }
        && requires(JsonWriter &writer, JsonReader &reader, const T &cv, T &v)
        {
            { JsonInterface<T>::write(writer, cv) } -> types::IsVoid;
            { JsonInterface<T>::read(reader, v) } -> types::IsVoid;
        };

    /// Check if the given type qualifies as a collection for the purposes
    /// of json manipulation.
    template<class T>
    concept IsCollection = requires(const T &value, size_t i)
    {
        {value.begin()} -> std::input_iterator;
        {value.end()} -> std::equality_comparable_with<decltype(value.begin())>;
        typename T::value_type;
    };
    
    /// Check if the given type qualifies as a map for the purposes
    /// of json manipulation.
    template<class T>
    concept IsMaplikeCollection = requires(const T &value, size_t i)
    {
        {value.begin()} -> std::input_iterator;
        {value.begin()->first} -> std::convertible_to<std::string_view>;
        value.begin()->second;
        {value.end()} -> std::equality_comparable_with<decltype(value.begin())>;
        typename T::value_type;
    };

    class JsonReader
    {
        struct Collection
        {
            bool is_object;

            Collection();
            Collection(bool obj);
        };

        std::istream *_stream;
        std::stack<Collection> _collections;
        bool _needs_key = false;

        void end_value();

    public:
        JsonReader();
        JsonReader(std::istream &_istream);

        bool at_quote();
        bool at_digit();
        bool at_null();
        bool at_open_object();
        bool at_close_object();
        bool at_open_array();
        bool at_close_array();

        void end();

        JsonReader &read_key(std::string &key);
        JsonReader &expect_key(std::string_view expect_key);

        template<class T>
        JsonReader &read_kvp(std::string &key, T &value)
        {
            read_key(key);
            read(value);
            return *this;
        }
        
        template<class T>
        JsonReader &expect_kvp(std::string_view key, T &value)
        {
            expect_key(key);
            read(value);
            return *this;
        }

        template<HasCustomInterface T>
        JsonReader &read(T &value)
        {
            if constexpr (requires{ JsonInterface<T>::read(*this, value); })
            {
                JsonInterface<T>::read(*this, value);
            }
            else JsonInterface<T>::handle(*this, value);

            return *this;
        }
        
        template<IsMaplikeCollection T>
        JsonReader &read(T &value)
        {
            using X = typename T::value_type;

            read_start_object();
            while(!at_close_object())
            {
                std::string key;
                read_key(key);

                X val = X();
                read(val);

                value[MOVE(key)] = val;
            }
            read_end_object();

            return *this;
        }

        JsonReader &read_null();

        JsonReader &read(int8_t &);
        JsonReader &read(int16_t &);
        JsonReader &read(int32_t &);
        JsonReader &read(int64_t &);

        JsonReader &read(uint8_t &);
        JsonReader &read(uint16_t &);
        JsonReader &read(uint32_t &);
        JsonReader &read(uint64_t &);
        
        JsonReader &read(float &);
        JsonReader &read(double &);
        
        JsonReader &read(bool &);
        
        JsonReader &read(std::string &);

        JsonReader &read(Object &);
        JsonReader &read(Array &);
        JsonReader &read(Any &);

        JsonReader &read_start_object();
        JsonReader &read_end_object();
        
        JsonReader &read_start_array();
        JsonReader &read_end_array();
    };

    class JsonWriter
    {
        struct Collection
        {
            bool after_first_object;
            bool is_object;

            Collection();
            Collection(bool obj);
        };

        std::ostream *_stream;
        std::stack<Collection> _collections;
        bool _needs_key = false;
        bool _pretty = false;
        int _indent = 0;

        void start_value();

        void start_value_pre();
        void start_value_post();

    public:
        JsonWriter();
        JsonWriter(std::ostream &stream, bool pretty=true);

        JsonWriter &write(Null);
        JsonWriter &write(int8_t);
        JsonWriter &write(int16_t);
        JsonWriter &write(int32_t);
        JsonWriter &write(int64_t);
        JsonWriter &write(uint8_t);
        JsonWriter &write(uint16_t);
        JsonWriter &write(uint32_t);
        JsonWriter &write(uint64_t);
        JsonWriter &write(float);
        JsonWriter &write(double);
        JsonWriter &write(bool);
        JsonWriter &write(std::string_view);
        JsonWriter &write(const std::string &);
        JsonWriter &write(const char *);
        JsonWriter &write(const Object &);
        JsonWriter &write(const Array &);
        JsonWriter &write(const Any &);

        template<HasCustomInterface T>
        JsonWriter &write(const T &value)
        {
            if constexpr (requires{ JsonInterface<T>::write(*this, value); })
            {
                JsonInterface<T>::write(*this, value);
            }
            else JsonInterface<T>::handle(*this, value);

            return *this;
        }

        JsonWriter &write_key(std::string_view);
        JsonWriter &write_key(const std::string &);
        JsonWriter &write_key(const char *);

        JsonWriter &write_start_object();
        JsonWriter &write_end_object();

        JsonWriter &write_start_array();
        JsonWriter &write_end_array();

        void set_pretty(bool pretty);
        bool is_pretty() const;

        // key-value-pair helper methods //
        template<class T>
        JsonWriter &write_kvp(std::string_view key, const T &value)
        {
            write_key(key);
            write(value);

            return *this;
        }
        
        template<class T>
        JsonWriter &write_kvp(const std::string &key, const T &value)
        {
            write_key(key);
            write(value);

            return *this;
        }
        
        template<class T>
        JsonWriter &write_kvp(const char *key, const T &value)
        {
            write_key(key);
            write(value);

            return *this;
        }

        // Wrappers for containers //        
        template<IsMaplikeCollection T>
        JsonWriter &write(const T &collection)
        {
            write_start_object();
            for(auto &[k, v] : collection)
            {
                write_key(k);
                write(v);
            }
            write_end_object();
            return *this;
        }
    };

    static inline void read(std::istream &stream, json::Any &any)
    {
        json::JsonReader jreader(stream);
        jreader.read(any);
    }
    
    static inline void write(std::ostream &stream, const json::Any &any)
    {
        json::JsonWriter jwriter(stream);
        jwriter.write(any);
    }

    /// Either a json reader or a json writer;
    /// this class should be used as the primary entrypoint for serializing datatypes,
    /// as it allows for easily merging writing and reading operations into one function,
    /// as well as fine-control for both writing and reading seperately if desired.
    class Json
    {
        std::variant<JsonReader *, JsonWriter *> inner;

    public:
        Json() : inner() {}
        Json(JsonReader &js) : inner(&js) {}
        Json(JsonWriter &js) : inner(&js) {}

        bool is_reader() const { return inner.index() == 0; }
        bool is_writer() const { return inner.index() == 1; }

        /// Split execution into read and write modes, where the first argument
        /// is a lambda called with the reader and the second is a lambda
        /// called with the writer.
        Json &visit(auto &&read, auto &&write)
        {
            if (is_reader()) std::invoke(read, *std::get<0>(inner));
            else std::invoke(write, *std::get<1>(inner));

            return *this;
        }

        /// Read or write a key from the json.
        Json &key(auto &&key)
        {
            return visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );
        }

        /// Read or write a value to the json.
        Json &val(auto &&val)
        {
            return visit(
                [&](JsonReader &js) { js.read(val); },
                [&](JsonWriter &js) { js.write(val); }
            );
        }

        /// Read or write a value to the json, using the provided 'encode' and 'decode'
        /// functions to convert to and from a representative object before serializing.
        Json &val(auto &&val, auto &&decode, auto &&encode)
        {
            using T = std::remove_cvref_t<decltype(encode(val))>;

            return visit(
                [&](JsonReader &js) { T v = T(); js.read(v); val = decode(v); },
                [&](JsonWriter &js) { js.write<T>(encode(val)); }
            );
        }

        /// Read or write a key and its associated value.
        Json &kvp(auto &&key, auto &&val)
        {
            return visit(
                [&](JsonReader &js) { js.expect_kvp(key, val); },
                [&](JsonWriter &js) { js.write_kvp(key, val); }
            );
        }

        /// Read or write a key, then split control into a lambda for reading the associated value
        /// and a lambda for writing the associated value.
        Json &kvp(auto &&key, auto &&read, auto &&write)
        {
            visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );

            return visit(FORWARD(read), FORWARD(write));
        }

        /// Read or write a key and its associated value, using the provided 'encode' and 'decode'
        /// functions to convert to and from a representative object before serializing.
        Json &kvp(auto &&key, auto &&val, auto &&encode, auto &&decode)
        {
            visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );

            return this->val(val, FORWARD(encode), FORWARD(decode));
        }

        /// Call the provided lambda for every element in the list-style container when writing
        /// or for every element in the json when reading.
        Json &list(auto &&l, auto &&each)
        {
            return visit(
                // When reading; read the start array, then apply the lambda to each element 
                // in the json before reading in the end of the array.
                [&](JsonReader &js) { 
                    js.read_start_array();
                    while (!js.at_close_array())
                    {
                        std::remove_reference_t<decltype(*l.begin())> elem;
                        each(*this, elem);

                        if constexpr(requires { l.push_back(elem); }) l.push_back(elem);
                        else if constexpr (requires { l.insert(l.end(), elem); }) l.insert(l.end(), elem);
                        else if constexpr (requires { l.insert(elem); }) l.insert(elem);
                    }
                    js.read_end_array();
                },

                // When writing; write the start array, then apply the lambda to each element 
                // in the list before writing the end of array.
                [&](JsonWriter &js) { 
                    js.write_start_array();
                    for (auto &elem : l)
                    {
                        each(*this, elem);
                    }
                    js.write_end_array();
                }
            );
        }

        /// Call the provided lambda for every element in the json-map-style container when writing
        /// or for every element in the json when reading. A json-map-style container is a container
        /// which can be iterated over to produce std::pair<(string-like), T>. The provided 'each'
        /// function is only called on the *values* of the map; the keys are always read in as 
        /// strings.
        Json &map(auto &&m, auto &&each)
        {
            return visit(
                // When reading, read the start and repeatedly read the key and value pairs
                // until exhausted, calling 'each' on the value only.
                [&](JsonReader &js) {
                    js.read_start_object();
                    while (!js.at_close_object())
                    {
                        std::string key;
                        decltype(m.begin()->second) elem;

                        js.read_key(key);
                        each(*this, elem);

                        m[MOVE(key)] = MOVE(elem);
                    }
                    js.read_end_object();
                },
                
                // When writing, write the start and repeatedly write the key and value pairs
                // until exhausted, calling 'each' on the value only.
                [&](JsonWriter &js) {
                    js.write_start_object();
                    for (auto &[key, elem] : m)
                    {
                        js.write_key(key);
                        each(*this, elem);
                    }
                    js.write_end_object();
                }
            );
        }

        /// Delegate to reading or writing as appropriate for the following.
        Json &start_array() { return visit(&JsonReader::read_start_array, &JsonWriter::write_start_array); }
        Json &start_object() { return visit(&JsonReader::read_start_object, &JsonWriter::write_start_object); }

        Json &end_array() { return visit(&JsonReader::read_end_array, &JsonWriter::write_end_array); }
        Json &end_object() { return visit(&JsonReader::read_end_object, &JsonWriter::write_end_object); }

        auto as_reader() { return is_reader() ? dib::option::Option<JsonReader &>(*std::get<0>(inner)) : dib::option::none; }
        auto as_writer() { return is_writer() ? dib::option::Option<JsonWriter &>(*std::get<1>(inner)) : dib::option::none; }
    };
    
    /// Supply the default implementations of JsonInterface<T>::write and JsonInterface<T>::read.

    template<class T>
    void JsonInterface<T>::write(JsonWriter &writer, const T &value)
    {
        writer.write(value);
    }
    
    template<class T>
    void JsonInterface<T>::read(JsonReader &reader, T &value)
    {
        reader.read(value);
    }

    /// Override the json interface for types which supply it as member functions.
    /// These types are required to either have a member .handle_json(Json &)
    /// or two members .write_to_json(JsonWriter &) const and .read_from_json(JsonReader &).
    template<std::derived_from<ProvidedJsonInterface> Type>
    struct JsonInterface<Type>
    {
        static void write(JsonWriter &writer, const Type &value)
        {
            if constexpr (requires{value.write_to_json(writer);})
            {
                value.write_to_json(writer);
            }
            else const_cast<Type &>(value).handle_json(Json{ writer });
        }

        static void read(JsonReader &reader, Type &value)
        {
            if constexpr (requires{value.read_from_json(reader);})
            {
                value.read_from_json(reader);
            }
            else value.handle_json(Json{ reader });
        }
    };

    /// Override the json interface for generic container types of compatible types
    template<collections::IsBasicList List>
    struct JsonInterface<List>
    {
        using Value = collections::ValueOf<List>;

        static void write(JsonWriter &writer, const List &value)
        {
            writer.write_start_array();
            for(auto &el : value)
            {
                writer.write(el);
            }
            writer.write_end_array();
        }

        static void read(JsonReader &reader, List &value)
        {
            if constexpr(!collections::IsInsertable<List>)
            {
                // For collections which do not support growing at runtime,
                // we need a temporary buffer to store elements.
                structures::Vector<Value> temp;

                reader.read_start_array();
                while(!reader.at_close_array())
                {
                    Value v; reader.read(v);
                    temp.push_back(MOVE(v));
                }
                reader.read_end_array();

                collections::reserve(value, temp.size());
                for(auto i = 0uz; i < temp.size(); i++)
                {
                    value[i] = MOVE(temp[i]);
                }
            }
            else
            {
                // Otherwise, we can read directly into the provided instance
                reader.read_start_array();
                while(!reader.at_close_array())
                {
                    Value v; reader.read(v);
                    collections::insert(value, MOVE(v));
                }
                reader.read_end_array();
            }
        }
    };  

    template<class T, size_t N>
    struct JsonInterface<dib::math::vec<T, N>>
    {
        static void write(json::JsonWriter &writer, const dib::math::vec<T, N> &value)
        {
            writer.write_start_array();
            template for(constexpr auto I : std::ranges::views::iota(0uz, N))
                writer.write(value.template get<I>());
            writer.write_end_array();
        }

        static void read(json::JsonReader &reader, dib::math::vec<T, N> &value)
        {
            reader.read_start_array();
            template for(constexpr auto I : std::ranges::views::iota(0uz, N))
                reader.read(value.template get<I>());
            reader.read_end_array();
        }
    };

    /// Override the json interface for the option type.
    template<class Type>
    struct JsonInterface<dib::option::Option<Type>>
    {
        static void write(JsonWriter &writer, const dib::option::Option<Type> &value)
        {
            if (value) writer.write(value.unwrap());
            else writer.write(null);
        }

        static void read(JsonReader &reader, dib::option::Option<Type> &value)
        {
            if (reader.at_null())
            {
                reader.read_null();
                value = dib::option::none;
            }
            else
            {
                Type val = Type(); reader.read(val);
                value = val;
            }
        }
    };
    
    template<AnnotatedDirectlyWith<DeriveJson> Type>
    struct JsonInterface<Type>
    {
        static void write(JsonWriter &writer, const Type &value)
        {
            if constexpr(std::is_enum_v<Type>)
            {
                template for(constexpr auto f : std::define_static_array(
                    std::meta::enumerators_of(^^Type)))
                {
                    if(value == [:f:])
                    {
                        writer.write(std::meta::identifier_of(f));
                    }
                }
            }
            else
            {
                writer.write_start_object();
                template for(constexpr auto f : std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked())))
                {
                    constexpr auto id = dib::annotation<Rename>(f)
                        .map([](auto &&r) { return r.to; })
                        .unwrap_or(std::define_static_string(std::meta::identifier_of(f)));

                    writer.write_kvp(id, value.[:f:]);
                }
                writer.write_end_object();
            }
        }
        
        static void read(JsonReader &reader, Type &value)
        {
            if constexpr(std::is_enum_v<Type>)
            {
                std::string str; reader.read(str);

                template for(constexpr auto f : std::define_static_array(
                    std::meta::enumerators_of(^^Type)))
                {
                    if(str == std::meta::identifier_of(f))
                    {
                        value = [:f:];
                    }
                }
            }
            else
            {
                reader.read_start_object();
                template for(constexpr auto f : std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unchecked())))
                {
                    constexpr auto id = dib::annotation<Rename>(f)
                        .map([](auto &&r) { return r.to; })
                        .unwrap_or(std::define_static_string(std::meta::identifier_of(f)));

                    reader.expect_kvp(id, value.[:f:]);
                }
                reader.read_end_object();
            }
        }
    };

    template<>
    struct JsonInterface<::Color>
    {
        static void write(JsonWriter &writer, const ::Color &value)
        {
            char buf[10];

            buf[0] = '#';
            buf[9] = 0;

            auto pnibl = [&](uint8_t nibl, uint8_t index)
            {
                if(nibl >= 10) buf[index] = 'a' + (nibl - 10);
                else buf[index] = '0' + nibl;
            };

            auto pbyte = [&](uint8_t byte, uint8_t index)
            {
                pnibl((byte & 0xF0) >> 4, index);
                pnibl((byte & 0x0F) >> 0, index + 1);
            };

            pbyte(value.r, 1);
            pbyte(value.g, 3);
            pbyte(value.b, 5);
            pbyte(value.a, 7);

            writer.write((const char *)buf);
        }

        static void read(JsonReader &reader, ::Color &value)
        {
            std::string str;
            reader.read(str);

            value = dib::literals::hex_color(str.c_str(), str.size());
        }
    };
    
    template<>
    struct JsonInterface<::Rectangle>
    {
        static void write(JsonWriter &js, const ::Rectangle &value)
        {
            js.write_start_array()
                .write(value.x)
                .write(value.y)
                .write(value.width)
                .write(value.height)
                .write_end_array();
        }

        static void read(JsonReader &js, ::Rectangle &value)
        {
            js.read_start_array()
                .read(value.x)
                .read(value.y)
                .read(value.width)
                .read(value.height)
                .read_end_array();
        }
    };

    /// Case-wise values; helper class
    /// Use this class to assist in the serialization of enumeration classes.
    template<class T>
    class EnumHelper
    {
    public:
        struct Reader
        {
            std::string e;
            T *value;
            bool success = false;

        public:
            Reader(JsonReader &js, T &value)
                : value(&value)
            {
                js.read(e);
            }

            void let_impl(const std::string_view &key, const T &target)
            {
                if (!success && e == key)
                {
                    *value = target;
                    success = true;
                }
            }

            void throw_impl(const std::string_view &msg)
            {
                RUNTIME_ERROR("Error reading json: {}", msg);
            }
        };

        struct Writer
        {
            JsonWriter *js;
            const T *value;
            bool success = false;

        public:
            Writer(JsonWriter &js, const T &value)
                : js(&js), value(&value)
            {}

            void let_impl(const std::string_view &key, const T &target)
            {
                if (!success && *value == target)
                {
                    js->write(key);
                    success = true;
                }
            }

            void throw_impl(const std::string_view &msg)
            {
                RUNTIME_ERROR("Error writing json: {}", msg);
            }
        };

    private:
        using var_t = std::variant<Reader, Writer>;
        var_t val;

    public:
        EnumHelper(JsonReader &js, T &val)
            : val(Reader{js, val})
        {}

        EnumHelper(JsonWriter &js, const T &val)
            : val(Writer{ js, val })
        {}

        EnumHelper(Json &js, T &val)
            : val(js.is_reader() ? var_t(Reader{ js.as_reader().unwrap(), val}) : var_t(Writer{ js.as_writer().unwrap(), val }))
        {}

        EnumHelper &let(const std::string_view &key, const T &target)
        {
            std::visit([&](auto &impl) {impl.let_impl(key, target);}, val);
            return *this;
        }

        bool good()
        {
            return std::visit([](auto &impl) {return impl.success;}, val);
        }

        void else_throw(const std::string_view &msg)
        {
            if(!good()) std::visit([&](auto &impl) {return impl.throw_impl(msg);}, val);
        }
    };

    template<class T>
    EnumHelper<T> enum_helper(JsonReader &js, T &value)
    {
        return EnumHelper<T>{ js, value };
    }

    template<class T>
    EnumHelper<T> enum_helper(JsonWriter &js, const T &value)
    {
        return EnumHelper<T>{ js, value };
    }

    template<class T>
    EnumHelper<T> enum_helper(Json &js, T &value)
    {
        return EnumHelper<T>{ js, value };
    }
}