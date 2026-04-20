#pragma once

#include <concepts>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <sstream>
#include <string_view>
#include <vector>
#include <list>
#include <typeindex>

#include "dib/debug.h"
#include "dib/ints.h"
#include "dib/json.h"
#include "dib/reflect.h"
#include "dib/types.h"

#include "raylib.h"

namespace dib::res
{
    class Resources;
}

namespace dib
{
    res::Resources &resources();
}

namespace dib::res
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

        unsigned char *file_load_data_callback(const char *filename, int *bytes_read);
        char *file_load_text_callback(const char *filename);
    }

    constexpr std::string_view raylib_resources = "resources/";
    
    /// Forward declare all the core resource types
    class Resources;
    class ResourceStore;
    class ResourceFolder;
    class ResourceBatch;
    template<class>
    class ResourceHandle;
    
    /// The interface which resource types must provide.
    template<class T>
    struct ResourceInterface
    {
        static void load(
            [[maybe_unused]] Resources &resources, 
            [[maybe_unused]] T &instance, 
            [[maybe_unused]] std::string_view filename, 
            [[maybe_unused]] const char *buffer, 
            [[maybe_unused]] size_t size) 
        {}
        static void unload(
            [[maybe_unused]] Resources &resources, 
            [[maybe_unused]] T &instance) 
        {}
        static void get_dependencies(
            [[maybe_unused]] std::vector<std::string> &deps)
        {}
        static constexpr bool free_underlying_once_loaded = false;

        using disable = void;
    };

    /// A class which provides easy access to information about a resource type,
    /// and provides reasonable defaults if a field is ommitted.
    template<class T>
    struct ResourceTraits
    {
        static_assert(!requires {typename ResourceInterface<T>::disable;}, "Cannot get the resource traits of a non-resource type!");

        static constexpr bool free_underlying_once_loaded = []
        {
            if constexpr(requires {{ResourceInterface<T>::free_underlying_once_loaded} -> std::convertible_to<bool>;})
            {
                return ResourceInterface<T>::free_underlying_once_loaded;
            }
            else 
            {
                return false;
            }
        }();

        static constexpr bool open_as_text = []
        {
            if constexpr (requires {{ResourceInterface<T>::open_as_text} -> std::convertible_to<bool>;})
            {
                return ResourceInterface<T>::open_as_text;
            }
            else
            {
                return false;
            }
        }();

        static void load(Resources &resources, T &instance, std::string_view filename, const char *buffer, size_t size)
        {
            if constexpr(requires {{ResourceInterface<T>::load(resources, instance, filename, buffer, size)} -> std::same_as<void>;})
            {
                ResourceInterface<T>::load(resources, instance, filename, buffer, size);
            }
        }
        
        static void unload(Resources &resources, T &instance)
        {
            if constexpr(requires {{ResourceInterface<T>::unload(resources, instance)} -> std::same_as<void>;})
            {
                ResourceInterface<T>::unload(resources, instance);
            }
        }
        
        static void get_dependencies(T &instance, std::vector<std::string> &deps)
        {
            if constexpr(requires {{ResourceInterface<T>::get_dependencies(instance, deps)} -> std::same_as<void>;})
            {
                ResourceInterface<T>::get_dependencies(instance, deps);
            }
        }
    };

    namespace detail
    {
        dib::res::ResourceStore *global_resource_store();

        /// The handle of a resource that has been loaded.
        class LoadedResource
        {
            Resources *_owner;
            std::type_index _type;
            size_t _re_size;
            char *_data;
            void (*_destructor)(Resources &, void *);
            std::vector<std::string> _dependencies;

            void move_from(LoadedResource &&other);
            void destruct();

            void free_underlying_buffer();

            template<class T>
            static LoadedResource load_with_type(Resources &owner, std::string_view filename, char *buffer, size_t file_size)
            {
                new(buffer) T();
                LoadedResource out(owner, typeid(T), sizeof(T), buffer, [](Resources &owner, void *p) 
                {
                    ResourceTraits<T>::unload(owner, *(T*)p);
                    ((T*)p)->~T();
                });

                ResourceTraits<T>::load(owner, *(T*)buffer, filename, buffer + sizeof(T), file_size);
                ResourceTraits<T>::get_dependencies(*(T*)buffer, out._dependencies);
                return out;
            }

            LoadedResource(Resources &resources, std::type_index type, size_t re_size, char *data, void (*destructor)(Resources &, void *));

        public:
            LoadedResource();
            LoadedResource(LoadedResource &&other) noexcept;

            ~LoadedResource();

            LoadedResource &operator=(LoadedResource &&other) noexcept;

            friend class dib::res::ResourceStore;
            friend class dib::res::ResourceBatch;
            friend class dib::res::ResourceFolder;
            template<class>
            friend class dib::res::ResourceHandle;
        };
    }

    /// A handle to a resource. Can perform either dynamic reloading or cached lookup
    /// depending on whether or not the resource storage system supports dynamic reloading
    /// of assets. Is also json serializable.
    template<class T>
    class [[=provides_hash]] ResourceHandle : public json::ProvidedJsonInterface
    {
        Resources *_owner;
        std::string _name;
        mutable const T *_cached;

        friend class dib::res::ResourceStore;
        friend class dib::res::Resources;
        
        ResourceHandle(Resources &owner, std::string_view name);

    public:
        ResourceHandle() : _owner(nullptr), _name(""), _cached(nullptr) {}

        bool is_valueless() const { return _owner == nullptr; }

        std::string_view name() const { return _name; }

        const T &get() const;

        const T &operator* () const { return get(); }
        const T *operator->() const { return std::addressof(get()); }

        void handle_json(json::Json &&js)
        {
            js.val(
                *this, 
                [](std::string str) { 
                    return ResourceHandle<T>(
                        resources(), 
                        //!WARNING! This undermines the idea of having multiple Resources instances!
                        str
                    ); 
                },
                [](const ResourceHandle<T> &handle) { return handle._name; }
            );
        }

        size_t get_hash() const 
        {
            return dib::get_hash(_name);
        }
    };

    /// The base class for all resource storage operations.
    class ResourceStore
    {
        template<class>
        friend class dib::res::ResourceHandle;
        friend class dib::res::Resources;

    protected:
        using Loader = detail::LoadedResource(*)(Resources &owner, std::string_view filename, char *buffer, size_t size);

        virtual const detail::LoadedResource &get_loaded(
            std::string_view name, Loader loader, size_t re_size, bool free_underlying_once_loaded, bool open_as_text) = 0;
        
        virtual bool requires_reload(std::string_view name) = 0;
        
        Resources *_owner = nullptr;
        void set_owner(Resources &owner);

    public:
        virtual ~ResourceStore() {}
        virtual bool supports_dynamic_reload() const = 0;
        virtual size_t get_content_size(std::string_view filename) const = 0;
        virtual void copy_content_to_buffer(std::string_view filename, void *buffer, bool open_as_text) const = 0;
    };

    /// The class responsible for the public-facing resources API.
    class Resources
    {
        std::unique_ptr<ResourceStore> _store;

        void assert_initialized() const;

    public:
        void set_store(std::unique_ptr<ResourceStore> &&store)
        {
            _store = MOVE(store);
            _store->set_owner(*this);
        }

        ResourceStore &store() 
        { 
            assert_initialized();
            return *_store; 
        }
        
        const ResourceStore &store() const
        {
            assert_initialized();
            return *_store; 
        }

        bool supports_dynamic_reload() const
        {
            return store().supports_dynamic_reload();
        }

        template<class T>
        ResourceHandle<T> get(std::string_view name)
        {
            assert_initialized();
            return ResourceHandle<T>(*this, name);
        }
    };

    template<class T>
    ResourceHandle<T>::ResourceHandle(Resources &owner, std::string_view name)
        : _owner(&owner)
        , _name(name)
        , _cached(nullptr)
    {
        if(!_owner->supports_dynamic_reload())
        {
            get();
        }
    }

    template<class T>
    const T &ResourceHandle<T>::get() const
    {
        if(is_valueless())
            RUNTIME_ERROR("Attempt to read from valueluess resource handle.");

        if(_cached)
            return *_cached;

        auto &resource = _owner->store().get_loaded(
            _name, 
            &detail::LoadedResource::load_with_type<T>, 
            sizeof(T), 
            ResourceTraits<T>::free_underlying_once_loaded, 
            ResourceTraits<T>::open_as_text);
            
        if(resource._type != typeid(T))
        {
            RUNTIME_ERROR(
                "Resource {} is of type {}, but is being read as type {}", 
                _name, resource._type.name(), refl::typeof<T>.name());
        }

        if(!_owner->supports_dynamic_reload())
        {
            _cached = (const T*)resource._data;
        }

        return *(const T*)resource._data;
    }

    class ResourceBatch final : public ResourceStore
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
        std::vector<detail::LoadedResource> loaded_resources;
        mutable std::fstream file;

        void parse_header();
        
        ResourceBatch(std::fstream &&file)
        {
            this->file = MOVE(file);
            parse_header();
        }

    protected:
        const detail::LoadedResource &get_loaded(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text) override;
        bool requires_reload(std::string_view name) override;

    public:
        ResourceBatch() {}

        static void make_from_directory(const std::filesystem::path &directory, const std::filesystem::path &output);
        void open(const std::filesystem::path &file);
        
        size_t get_content_size(std::string_view filename) const override;
        void copy_content_to_buffer(std::string_view filename, void *buffer, bool open_as_text) const override;
        bool supports_dynamic_reload() const override { return false; }
    };

    class ResourceFolder final : public ResourceStore
    {
        struct TimeOfLoad
        {
            u64 frame_loaded;
            u64 frame_last_checked;
            std::chrono::milliseconds sys_time;
        };

        std::list<std::string> name_storage;
        std::unordered_map<std::string_view, detail::LoadedResource> loaded_resources;
        std::unordered_map<std::string_view, TimeOfLoad> resource_time_of_load;
        std::filesystem::path folder;

    protected:
        const detail::LoadedResource &get_loaded(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text) override;
        bool requires_reload(std::string_view name) override;

    public:
        ResourceFolder(std::filesystem::path folder)
            : folder(folder)
        {}
        
        size_t get_content_size(std::string_view filename) const override;
        void copy_content_to_buffer(std::string_view filename, void *buffer, bool open_as_text) const override;
        bool supports_dynamic_reload() const override { return true; }
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

    // Buffer resource //
    class Buffer
    {
        const char *_data;
        size_t _size;
        
        friend struct ResourceInterface<Buffer>;
        Buffer(const char *data, size_t size);

    public:
        Buffer();

        size_t size() const;
        const char *data() const;
    };

    /// Helper resource class which is defined using a JSON,
    /// which is then deserialized upon loading.
    class JsonResource {};
    constexpr JsonResource json_resource;

    template<class T>
    concept IsJsonResource = AnnotatedDirectlyWith<T, JsonResource>;

    // Default implementations of ResourceInterface //
    template<> struct ResourceInterface<Text>
    {
        static void load(
            [[maybe_unused]] Resources &resources, 
            [[maybe_unused]] Text &instance, 
            [[maybe_unused]] std::string_view filename, 
            [[maybe_unused]] const char *buffer, 
            [[maybe_unused]] size_t size);

        static constexpr bool open_as_text = true;
    };

    template<> struct ResourceInterface<Buffer>
    {
        static void load(
            [[maybe_unused]] Resources &resources, 
            [[maybe_unused]] Buffer &instance, 
            [[maybe_unused]] std::string_view filename, 
            [[maybe_unused]] const char *buffer, 
            [[maybe_unused]] size_t size);
            
        static constexpr bool free_underlying_once_loaded = false;
    };

    template<IsJsonResource T>
    struct ResourceInterface<T>
    {
        static void load(Resources &, T &instance, std::string_view, const char *buffer, size_t)
        {
            std::stringstream buffer_stream(buffer);
            json::JsonReader json_read(buffer_stream);

            json_read.read(instance);
        }

        static constexpr bool free_underlying_once_loaded = true;
        static constexpr bool open_as_text = true;
    };
    
    template<> struct ResourceInterface<::Image>
    {
        static void load(Resources &resources, ::Image &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, ::Image &instance);
        
        static constexpr bool free_underlying_once_loaded = true;
    };
    
    template<> struct ResourceInterface<::Texture>
    {
        static void load(Resources &resources, ::Texture &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, ::Texture &instance);
        
        static constexpr bool free_underlying_once_loaded = true;
    };
    
    template<> struct ResourceInterface<::Music>
    {
        static void load(Resources &resources, ::Music &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, ::Music &instance);
    };
    
    template<> struct ResourceInterface<::Sound>
    {
        static void load(Resources &resources, ::Sound &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, ::Sound &instance);
    };
    
    template<> struct ResourceInterface<::Model>
    {
        static void load(Resources &resources, ::Model &instance, std::string_view filename, const char *buffer, size_t size);
        static void unload(Resources &resources, ::Model &instance);
    };
}