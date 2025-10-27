#ifndef __RESOURCES_H
#define __RESOURCES_H

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <string_view>
#include <memory>
#include <vector>
#include <exception>
#include <list>
#include <typeindex>

#include "raw_memory.h"
#include "raylib.h"

namespace dib::resources
{
    namespace detail
    {
        struct ResourceKey
        {
            std::string_view identifier;
            uint64_t size_bytes = 0;
            uint64_t offset_bytes = 0;
            uint32_t salt = 0;

            uint32_t index = 0;
        };

        void salt_buffer(char *buffer, uint64_t size, uint32_t salt);
        void unsalt_buffer(char *buffer, uint64_t size, uint32_t salt);
        void read_big_endian(std::fstream &stream, char *buffer, size_t size);
        void write_big_endian(std::fstream &stream, const char *buffer, size_t size);

        template<class T>
        void read_big_endian(std::fstream &stream, T *buffer)
        {
            read_big_endian(stream, (char*)buffer, sizeof(T));
        }
        
        template<class T>
        void write_big_endian(std::fstream &stream, const T *buffer)
        {
            write_big_endian(stream, (const char*)buffer, sizeof(T));
        }
    }

    class ResourceException : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };
    
    template<class T>
    struct ResourceInterface
    {
        static void load(T &instance, std::string_view filename, const char *buffer, size_t size) {}
        static void unload(T &instance) {}
        static constexpr bool shrink_on_construction = false;

        using disable = void;
    };

    template<class T>
    struct ResourceTraits
    {
        static_assert(!requires {typename ResourceInterface<T>::disable;}, "Cannot get the resource traits of a non-resource type!");

        static constexpr bool shrink_on_construction = []
        {
            if constexpr(requires {{ResourceInterface<T>::shrink_on_construction} -> std::same_as<bool>;})
            {
                return ResourceInterface<T>::shrink_on_construction;
            }
            else 
            {
                return false;
            }
        }();

        static constexpr bool open_as_text = []
        {
            if constexpr (requires {{ResourceInterface<T>::open_as_text} -> std::same_as<bool>;})
            {
                return ResourceInterface<T>::open_as_text;
            }
            else
            {
                return false;
            }
        }();

        static void load(T &instance, std::string_view filename, const char *buffer, size_t size)
        {
            if constexpr(requires {{ResourceInterface<T>::load(instance, filename, buffer, size)} -> std::same_as<void>;})
            {
                ResourceInterface<T>::load(instance, filename, buffer, size);
            }
        }
        
        static void unload(T &instance)
        {
            if constexpr(requires {{ResourceInterface<T>::unload(instance)} -> std::same_as<void>;})
            {
                ResourceInterface<T>::unload(instance);
            }
        }
    };

    class Resource
    {
        std::type_index _type;
        size_t _re_size;
        char *_data;
        void (*_destructor)(void *);

        void move_from(Resource &&other);
        void destruct();

        void shrink();

        template<class T>
        static Resource load_from(std::string_view filename, char *buffer, size_t file_size)
        {
            new(buffer) T();
            Resource out(typeid(T), sizeof(T), buffer, [](void *p) 
            {
                ResourceTraits<T>::unload(*(T*)p);
                ((T*)p)->~T();
            });

            ResourceTraits<T>::load(*(T*)buffer, filename, buffer + sizeof(T), file_size);

            return out;
        }

        Resource(std::type_index type, size_t re_size, char *data, void (*destructor)(void *));

    public:
        Resource();
        Resource(Resource &&other) noexcept;

        ~Resource();

        Resource &operator=(Resource &&other) noexcept;

        friend class Resources;
        friend class ResourceBatch;
        friend class ResourceFolder;

        template<class T>
        const T &as() const
        {
        #ifndef NDEBUG

            if(_type != typeid(T))
            {
                std::cerr << "Attempt to access a resource with incorrect datatype." << std::endl;
                std::abort();
            }

        #endif

            return *(T*)_data;
        }
    };

    class Resources
    {
    protected:
        using Loader = Resource(*)(std::string_view filename, char *buffer, size_t size);
        virtual const Resource &get(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text) = 0;

    public:
        virtual ~Resources() {}

        template<class T>
        const T &get(std::string_view name)
        {
            auto &resource = get(name, &Resource::load_from<T>, sizeof(T), ResourceTraits<T>::shrink_on_construction, ResourceTraits<T>::open_as_text);
            return resource.template as<T>();
        }
    };

    class ResourceBatch final : public Resources
    {
        // FILE FORMAT //

        // big endian
        // first 7 bytes: "dibbatch" in ascii

        // next bytes: resource count (32 bit length)
        // next bytes: resource keys
            // resource key: identifier (32 bit length, ascii contents)
            //               size (64 bit)
            //               offset from start of file (64 bit)
            //               salt (32 bit)
        
        // subsequent bytes: each file, whose contents have been bitwise-xor'd with the provided salt
        //                   to provide rudementary obfuscation.

        std::list<std::string> name_storage;
        std::unordered_map<std::string_view, detail::ResourceKey> resource_keys;
        std::vector<Resource> loaded_resources;
        std::fstream file;

        void parse_header();
        
        ResourceBatch(std::fstream &&file)
        {
            this->file = std::move(file);
            parse_header();
        }

    protected:
        const Resource &get(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text) override;

    public:
        ResourceBatch() {}

        static void make_from_directory(const std::filesystem::path &directory, const std::filesystem::path &output);
        void open(const std::filesystem::path &file);
    };

    class ResourceFolder final : public Resources
    {
        std::list<std::string> name_storage;
        std::unordered_map<std::string_view, Resource> loaded_resources;
        std::filesystem::path folder;

    protected:
        const Resource &get(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text) override;

    public:
        ResourceFolder(std::filesystem::path folder)
            : folder(folder)
        {}
    };

    std::filesystem::path resource_batch_location();

    // Text resource //
    class Text
    {
        const char *_data;
        size_t _size;

        friend struct ResourceInterface<Text>;
        Text(const char *data, size_t size);

    public:
        Text();

        size_t size() const;
        std::string_view str() const;
        const char *c_str() const;
    };

    // Default implementations of ResourceInterface //
    template<> struct ResourceInterface<Text>
    {
        static void load(Text &instance, std::string_view filename, const char *buffer, size_t size);

        static constexpr bool open_as_text = true;
    };
    
    template<> struct ResourceInterface<::Image>
    {
        static void load(::Image &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(::Image &instance);
        
        static constexpr bool shrink_on_construction = true;
    };
    
    template<> struct ResourceInterface<::Texture>
    {
        static void load(::Texture &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(::Texture &instance);
        
        static constexpr bool shrink_on_construction = true;
    };
    
    template<> struct ResourceInterface<::Music>
    {
        static void load(::Music &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(::Music &instance);
    };
    
    template<> struct ResourceInterface<::Sound>
    {
        static void load(::Sound &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(::Sound &instance);
    };
}

#endif