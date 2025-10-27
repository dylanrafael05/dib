#include "dib/resources.h"
#include "dib/debug.h"
#include "dib/env.h"

#include <bit>
#include <array>
#include <cstring>
#include <algorithm>
#include <numeric>

namespace fs = std::filesystem;
using namespace std;
using namespace std::string_view_literals;
using namespace dib;
using namespace dib::resources;
using namespace dib::resources::detail;

// Helper methods //
void dib::resources::detail::read_big_endian(fstream &stream, char *buffer, size_t size)
{
    stream.read(buffer, size);

    if constexpr(endian::native == endian::little)
    {
        std::reverse(buffer, buffer + size);
    }
}

void dib::resources::detail::write_big_endian(fstream &stream, const char *buffer, size_t size)
{
    if constexpr(endian::native == endian::big)
    {
        stream.write(buffer, size);
    }
    else 
    {
        for(auto it = buffer + size - 1; it != buffer - 1; it--)
        {
            stream.write(it, 1);
        }
    }
}

void dib::resources::detail::salt_buffer(char *buffer, uint64_t size, uint32_t salt)
{
    auto salt_arr = bit_cast<array<char, 4>>(salt);

    uint32_t positional_salt = salt;
    auto salt_it = salt_arr.begin();

    for(auto it = buffer; it != buffer + size; it++, salt_it++)
    {
        if(salt_it == salt_arr.end()) salt_it = salt_arr.begin();

        *it = *it ^ *salt_it ^ positional_salt;

        positional_salt = (positional_salt + (positional_salt << 3) + 13) & std::numeric_limits<uint32_t>::max();
        positional_salt = (positional_salt - positional_salt * positional_salt * 7) & std::numeric_limits<uint32_t>::max();
    }
}

void dib::resources::detail::unsalt_buffer(char *buffer, uint64_t size, uint32_t salt)
{
    salt_buffer(buffer, size, salt);
}

// Resource //
void Resource::move_from(Resource &&other)
{
    _type = other._type;
    _re_size = other._re_size;
    _data = other._data;
    _destructor = other._destructor;

    other._data = nullptr;
}

void Resource::destruct()
{
    if(_data)
    {
        _destructor(_data);

        delete[] _data;
    }
}

Resource::Resource()
    : _type(typeid(nullptr))
    , _re_size(0)
    , _data(nullptr)
    , _destructor(nullptr)
{}

Resource::Resource(Resource &&other) noexcept
    : _type(typeid(nullptr))
{
    move_from(std::move(other));
}

Resource::~Resource()
{
    destruct();
}

Resource &Resource::operator=(Resource &&other) noexcept
{
    destruct();
    move_from(std::move(other));
    return *this;
}

Resource::Resource(std::type_index type, size_t re_size, char *data, void (*destructor)(void *))
    : _type(type)
    , _re_size(re_size)
    , _data(data)
    , _destructor(destructor)
{}

void Resource::shrink()
{
    char *new_data = new char[_re_size];
    std::memcpy(new_data, _data, _re_size);

    delete[] _data;
    _data = new_data;
}

// ResourceBatch //
void dib::resources::ResourceBatch::parse_header()
{
    file.seekg(0);

    // Reject file if not a resource batch //
    char signature[9];
    std::fill_n(signature, 9, 0);
    file.read(signature, 8);

    if(signature != "dibbatch"sv)
    {
        throw ResourceException{"Attempted to open a resource file which did not have the proper signature."};
    }

    // Parse resource headers //
    uint32_t resource_count;
    read_big_endian(file, &resource_count);

    for(uint32_t i = 0; i < resource_count; i++)
    {
        // Read in variables //
        uint32_t name_length;
        read_big_endian(file, &name_length);

        char* name = new char[name_length + 1];
        std::fill_n(name, name_length + 1, 0);

        file.read(name, name_length);

        uint64_t size;
        read_big_endian(file, &size);
        
        uint64_t offset;
        read_big_endian(file, &offset);
        
        uint32_t salt;
        read_big_endian(file, &salt);

        // Append to internal map //
        name_storage.push_back(string(name));
        auto view = string_view(name_storage.back());

        resource_keys[view] = ResourceKey{
            .identifier = view,
            .size_bytes = size,
            .offset_bytes = offset,
            .salt = salt,
            .index = (uint32_t)(-1),
        };

        delete[] name;
    }
}

const Resource &dib::resources::ResourceBatch::get(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text)
{
    auto key_it = resource_keys.find(name);
    if(key_it == resource_keys.end())
    {
        throw ResourceException{"Attempt to open resource which does not exist."};
    }

    auto &key = key_it->second;

    if(key.index == (uint32_t)(-1))
    {
        key.index = (uint32_t)loaded_resources.size();

        char *buffer = new char[key.size_bytes + re_size];
        file.seekg(key.offset_bytes);
        file.read(buffer + re_size, key.size_bytes);
        
        unsalt_buffer(buffer + re_size, key.size_bytes, key.salt);

        loaded_resources.push_back(loader(name, buffer, key.size_bytes));

        if(shrink)
        {
            loaded_resources.back().shrink();
        }
    }

    return loaded_resources[key.index];
}

void dib::resources::ResourceBatch::make_from_directory(const fs::path &directory, const fs::path &output)
{
    fstream output_file(output, ios::out | ios::binary);
    output_file.write("dibbatch", 8);

    std::list<std::string> name_storage;
    std::vector<ResourceKey> resources;

    // Create all resource keys //
    for(auto &file : fs::recursive_directory_iterator{directory})
    {
        if(file.is_directory()) continue;

        auto size = file.file_size();
        auto path = fs::relative(file.path(), directory).string();

        name_storage.push_back(std::move(path));
        resources.push_back(ResourceKey{
            .identifier = string_view{name_storage.back()},
            .size_bytes = size,
            .offset_bytes = 0,
            .salt = (uint32_t)hash<string>{}(name_storage.back()),
            .index = (uint32_t)(-1)
        });
    }

    auto resource_count = (uint32_t)resources.size();
    write_big_endian(output_file, &resource_count);
    
    // Calculate resource key offsets and write headers to file //
    size_t accumulated_offset = 8 + sizeof(uint32_t);
    for(auto &resource : resources)
    {
        accumulated_offset += sizeof(uint32_t)  // name length
                            + sizeof(uint64_t)  // size bytes
                            + sizeof(uint64_t)  // offset bytes
                            + sizeof(uint32_t); // salt

        accumulated_offset += resource.identifier.size();
    }

    for(auto &resource : resources)
    {
        resource.offset_bytes = accumulated_offset;
        accumulated_offset += resource.size_bytes;
        
        uint32_t size = (uint32_t)resource.identifier.size();
        write_big_endian(output_file, &size);

        output_file.write(resource.identifier.data(), size);

        write_big_endian(output_file, &resource.size_bytes);
        write_big_endian(output_file, &resource.offset_bytes);
        write_big_endian(output_file, &resource.salt);
    }

    // Write salted buffers to file //
    for(auto &key : resources)
    {
        ifstream stream(directory / key.identifier);

        char *buffer = new char[key.size_bytes];
        stream.read(buffer, key.size_bytes);

        salt_buffer(buffer, key.size_bytes, key.salt);
        output_file.write(buffer, key.size_bytes);

        delete[] buffer;
    }
}

void dib::resources::ResourceBatch::open(const fs::path &file)
{
    if(this->file.is_open())
    {
        std::cerr << "Cannot open an already opened resource batch." << std::endl;
        std::abort();
    }

    this->file.open(file, std::ios::in | std::ios::binary);
}

fs::path dib::resources::resource_batch_location()
{
    return dib::env::executable_directory_path() / ".dbatch";
}

// ResourceFolder //
const Resource &dib::resources::ResourceFolder::get(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text)
{
    auto it = loaded_resources.find(name);

    if(it == loaded_resources.end())
    {
        auto filename = folder / name;
        auto stream = text ? ifstream(filename) : ifstream(filename, ios::binary);

        if(stream.bad())
        {
            throw ResourceException{"Attempt to open resource which does not exist."};
        }

        auto size = fs::file_size(filename);
        auto buffer = new char[size + re_size];

        stream.read(buffer + re_size, size);

        auto resource = loader(name, buffer, size);

        name_storage.emplace_back(name);
        it = loaded_resources.insert({name_storage.back(), std::move(resource)}).first;

        if(shrink)
        {
            it->second.shrink();
        }
    }

    return it->second;
}

// Text resources //
Text::Text() 
    : _data(""), _size(0)
{}

Text::Text(const char *ptr, size_t size)
    : _data(ptr), _size(size)
{}

std::string_view Text::str() const
{
    return {_data, _size};
}

const char *Text::c_str() const
{
    return _data;
}

size_t Text::size() const
{
    return _size;
}

void ResourceInterface<Text>::load(Text &instance, std::string_view, const char *buffer, size_t size)
{
    instance = {buffer, size};
}

// Image //
void ResourceInterface<::Image>::load(::Image &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    instance = LoadImageFromMemory(ext_str.c_str(), (const unsigned char *)buffer, (int)size);
}

void ResourceInterface<::Image>::unload(::Image &instance)
{
    UnloadImage(instance);
}

// Texture //
void ResourceInterface<::Texture>::load(::Texture &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    auto img = LoadImageFromMemory(ext_str.c_str(), (const unsigned char *)buffer, (int)size);
    instance = LoadTextureFromImage(img);
    UnloadImage(img);
}

void ResourceInterface<::Texture>::unload(::Texture &instance)
{
    UnloadTexture(instance);
}

// Music //
void ResourceInterface<::Music>::load(::Music &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    instance = LoadMusicStreamFromMemory(ext_str.c_str(), (const unsigned char*)buffer, (int)size);
}

void ResourceInterface<::Music>::unload(::Music &instance)
{
    UnloadMusicStream(instance);
}

// Sound //
void ResourceInterface<::Sound>::load(::Sound &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    auto wave = LoadWaveFromMemory(ext_str.c_str(), (const unsigned char*)buffer, (int)size);
    instance = LoadSoundFromWave(wave);
    UnloadWave(wave);
}

void ResourceInterface<::Sound>::unload(::Sound &instance)
{
    UnloadSound(instance);
}