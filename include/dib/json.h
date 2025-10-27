#ifndef __DIBJSON_H
#define __DIBJSON_H

#include <memory>
#include <vector>
#include <stack>
#include <unordered_map>
#include <exception>
#include <stdexcept>
#include <variant>

#include <cstdint>
#include <string>
#include <string_view>
#include <iosfwd>

#include <concepts>

#include "dib/types.h"
#include "dib/optional.h"

namespace dib::json
{
    using Int = int64_t;
    using Bool = bool;
    using Float = float;
    using String = std::string;
    using Null = decltype(nullptr);
    
    struct Object;
    struct Array;

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

    enum class JsonError
    {
        none,
        unmatched_object_open,
        unmatched_array_open,
        unterminated_string,
        unknown_character,
        unexpected_character,
        unmatched_object_key,
        invalid_key
    };

    JsonError read(std::istream &stream, Any &output);

    // TODO: handle escaped strings //

    class JsonException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    
    class JsonReadException : public std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    class JsonWriter;
    class JsonReader;

    template<class T>
    struct JsonInterface
    {
        static void write(JsonWriter &writer, const T &value);
        static void read(JsonReader &reader, T &value);

        using is_default = void;
    };

    struct ProvidedJsonInterface : types::Marker<ProvidedJsonInterface> {};

    template<class T>
    concept has_custom_write = !requires 
    {
        typename JsonInterface<T>::is_default;
    };

    template<class T>
    concept collection = requires(const T &value, size_t i)
    {
        {value.begin()} -> std::input_iterator;
        {value.end()} -> std::equality_comparable_with<decltype(value.begin())>;
        typename T::value_type;
    };
    
    template<class T>
    concept maplike_collection = requires(const T &value, size_t i)
    {
        {value.begin()} -> std::input_iterator;
        {value.begin()->first} -> std::convertible_to<std::string_view>;
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

        template<has_custom_write T>
        JsonReader &read(T &value)
        {
            if constexpr (requires{ JsonInterface<T>::read(*this, value); })
            {
                JsonInterface<T>::read(*this, value);
            }
            else JsonInterface<T>::handle(*this, value);

            return *this;
        }

        template<collection T> requires (!maplike_collection<T>)
        JsonReader &read(T &value)
        {
            using X = typename T::value_type;

            read_start_array();
            while(!at_close_array())
            {
                X val = X();
                read(val);

                value.push_back(val);
            }
            read_end_array();

            return *this;
        }
        
        template<maplike_collection T>
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

                value[std::move(key)] = val;
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

        template<has_custom_write T>
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
        template<collection T> requires (!maplike_collection<T>)
        JsonWriter &write(const T &collection)
        {
            write_start_array();
            for(auto &e : collection)
            {
                write(e);
            }
            write_end_array();
            return *this;
        }
        
        template<maplike_collection T>
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

    class Json
    {
        std::variant<JsonReader *, JsonWriter *> inner;

    public:
        Json() : inner() {}
        Json(JsonReader &js) : inner(&js) {}
        Json(JsonWriter &js) : inner(&js) {}

        bool is_reader() const { return inner.index() == 0; }
        bool is_writer() const { return inner.index() == 1; }

        Json &visit(auto &&read, auto &&write)
        {
            if (is_reader()) std::invoke(read, *std::get<0>(inner));
            else std::invoke(write, *std::get<1>(inner));

            return *this;
        }

        Json &key(auto &&key)
        {
            return visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );
        }

        Json &val(auto &&val)
        {
            return visit(
                [&](JsonReader &js) { js.read(val); },
                [&](JsonWriter &js) { js.write(val); }
            );
        }

        Json &val(auto &&val, auto &&decode, auto &&encode)
        {
            using T = std::remove_cvref_t<decltype(encode(val))>;

            return visit(
                [&](JsonReader &js) { T v = T(); js.read(v); val = decode(v); },
                [&](JsonWriter &js) { js.write<T>(encode(val)); }
            );
        }

        Json &kvp(auto &&key, auto &&val)
        {
            return visit(
                [&](JsonReader &js) { js.expect_kvp(key, val); },
                [&](JsonWriter &js) { js.write(key, val); }
            );
        }

        Json &kvp(auto &&key, auto &&read, auto &&write)
        {
            visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );

            return visit(DIB_FWD(read), DIB_FWD(write));
        }

        Json &kvp(auto &&key, auto &&val, auto &&encode, auto &&decode)
        {
            visit(
                [&](JsonReader &js) { js.expect_key(key); },
                [&](JsonWriter &js) { js.write_key(key); }
            );

            return this->val(val, DIB_FWD(encode), DIB_FWD(decode));
        }

        Json &list(auto &&l, auto &&each)
        {
            return visit(
                [&](JsonReader &js) { 
                    js.read_start_array();
                    while (!js.at_close_array())
                    {
                        std::remove_reference_t<decltype(*l.begin())> elem;
                        each(*this, elem);

                        if constexpr(requires { l.push_back(elem); }) l.push_back(elem);
                        else if constexpr (requires { l.insert(l.end(), elem); }) l.push_back(elem);
                        else if constexpr (requires { l.insert(elem); }) l.insert(elem);
                    }
                    js.read_end_array();
                },
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

        Json &map(auto &&m, auto &&each)
        {
            return visit(
                [&](JsonReader &js) {
                    js.read_start_object();
                    while (!js.at_close_object())
                    {
                        std::string key;
                        decltype(m.begin()->second) elem;

                        js.read_key(key);
                        each(*this, elem);

                        m[std::move(key)] = std::move(elem);
                    }
                    js.read_end_object();
                },
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

        Json &start_array() { return visit(&JsonReader::read_start_array, &JsonWriter::write_start_array); }
        Json &start_object() { return visit(&JsonReader::read_start_object, &JsonWriter::write_start_object); }

        Json &end_array() { return visit(&JsonReader::read_end_array, &JsonWriter::write_end_array); }
        Json &end_object() { return visit(&JsonReader::read_end_object, &JsonWriter::write_end_object); }

        auto as_reader() { return is_reader() ? dib::Optional<JsonReader &>(*std::get<0>(inner)) : dib::none; }
        auto as_writer() { return is_writer() ? dib::Optional<JsonWriter &>(*std::get<1>(inner)) : dib::none; }
    };
    
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

    template<class Type>
    struct JsonInterface<dib::Optional<Type>>
    {
        static void write(JsonWriter &writer, const dib::Optional<Type> &value)
        {
            if (value) writer.write(*value);
            else writer.write(null);
        }

        static void read(JsonReader &reader, dib::Optional<Type> &value)
        {
            if (reader.at_null())
            {
                Null n = null; reader.read(n);
                value = dib::none;
            }
            else
            {
                Type val = Type(); reader.read(val);
                value = val;
            }
        }
    };

    // Case-wise values; helper class //
    template<class T>
    class CaseHelper
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
                throw JsonReadException{ msg.data() };
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
                throw JsonException{ msg.data() };
            }
        };

    private:
        using var_t = std::variant<Reader, Writer>;
        var_t val;

    public:
        CaseHelper(JsonReader &js, T &val)
            : val(Reader{js, val})
        {}

        CaseHelper(JsonWriter &js, const T &val)
            : val(Writer{ js, val })
        {}

        CaseHelper(Json &js, T &val)
            : val(js.is_reader() ? var_t(Reader{ *js.as_reader(), val}) : var_t(Writer{ *js.as_writer(), val }))
        {}

        CaseHelper &let(const std::string_view &key, const T &target)
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
    CaseHelper<T> case_helper(JsonReader &js, T &value)
    {
        return CaseHelper<T>{ js, value };
    }

    // Case-wise writing; write helper //
    template<class T>
    CaseHelper<T> case_helper(JsonWriter &js, const T &value)
    {
        return CaseHelper<T>{ js, value };
    }

    template<class T>
    CaseHelper<T> case_helper(Json &js, T &value)
    {
        return CaseHelper<T>{ js, value };
    }
}

#endif