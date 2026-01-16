#include "dib/resources.h"
#include "dib/debug.h"
#include "dib/env.h"
#include "dib/app.h"
#include "raylib.h"

#include <bit>
#include <array>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <filesystem>

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

template<bool AsText>
auto file_load_callback(
    const char *filename_c, int *bytes_read, 
    auto &&callback_setter, auto &&callback_self)
{
    std::string_view filename = filename_c;

    if(filename.starts_with(resources::raylib_resources))
    {
        auto resource_name = filename.substr(resources::raylib_resources.size());
        auto &resources = dib::app::this_app().resources();
        auto resource_size = resources.store().get_content_size(resource_name);

        *bytes_read = (int) resource_size;
        auto buffer = RL_CALLOC(resource_size + 1, sizeof(char));
        
        resources.store().copy_content_to_buffer(resource_name, buffer, AsText);
        return buffer;
    }

    callback_setter(nullptr);

    if constexpr(AsText)
    {
        auto result = LoadFileText(filename_c);
        callback_setter(callback_self);
        return (void *) result;
    }
    else 
    {
        auto result = LoadFileData(filename_c, bytes_read);
        callback_setter(callback_self);
        return (void *) result;
    }
}

unsigned char *dib::resources::detail::file_load_data_callback(const char *filename_c, int *bytes_read)
{
    return (unsigned char *)file_load_callback<false>(filename_c, bytes_read, SetLoadFileDataCallback, file_load_data_callback);
}

char *dib::resources::detail::file_load_text_callback(const char *filename_c)
{
    int dummy;
    return (char *)file_load_callback<true>(filename_c, &dummy, SetLoadFileDataCallback, file_load_data_callback);
}

// Resource //
ResourceStore *detail::global_resource_store()
{
    return &dib::app::this_app().resources().store();
}

void detail::LoadedResource::move_from(detail::LoadedResource &&other)
{
    _type = other._type;
    _re_size = other._re_size;
    _data = other._data;
    _destructor = other._destructor;
    _owner = other._owner;

    other._data = nullptr;
}

void detail::LoadedResource::destruct()
{
    if(_data)
    {
        _destructor(*_owner, _data);

        delete[] _data;
    }
}

detail::LoadedResource::LoadedResource()
    : _owner(nullptr)
    , _type(typeid(nullptr))
    , _re_size(0)
    , _data(nullptr)
    , _destructor(nullptr)
{}

detail::LoadedResource::LoadedResource(detail::LoadedResource &&other) noexcept
    : _type(typeid(nullptr))
{
    move_from(MOVE(other));
}

detail::LoadedResource::~LoadedResource()
{
    destruct();
}

detail::LoadedResource &detail::LoadedResource::operator=(detail::LoadedResource &&other) noexcept
{
    destruct();
    move_from(std::move(other));
    return *this;
}

detail::LoadedResource::LoadedResource(
    Resources &owner, std::type_index type, 
    size_t re_size, char *data, 
    void (*destructor)(Resources &, void *))
    : _owner(&owner)
    , _type(type)
    , _re_size(re_size)
    , _data(data)
    , _destructor(destructor)
{}

void detail::LoadedResource::free_underlying_buffer()
{
    char *new_data = new char[_re_size];
    std::memcpy(new_data, _data, _re_size);

    delete[] _data;
    _data = new_data;
}

// ResourceStore //
void dib::resources::ResourceStore::set_owner(dib::resources::Resources &owner)
{
    if(_owner)
        RUNTIME_ERROR("Setting owner of resource store more than once!");

    _owner = &owner;
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
        RUNTIME_ERROR("Attempted to open a resource file which did not have the proper signature.");
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

const detail::LoadedResource &dib::resources::ResourceBatch::get_loaded(std::string_view name, Loader loader, size_t re_size, bool shrink, bool)
{
    auto key_it = resource_keys.find(name);
    if(key_it == resource_keys.end())
    {
        RUNTIME_ERROR("Attempt to open resource which does not exist.");
    }

    auto &key = key_it->second;

    if(key.index == (uint32_t)(-1))
    {
        key.index = (uint32_t)loaded_resources.size();

        char *buffer = new char[key.size_bytes + re_size];
        file.seekg(key.offset_bytes);
        file.read(buffer + re_size, key.size_bytes);
        
        unsalt_buffer(buffer + re_size, key.size_bytes, key.salt);

        loaded_resources.push_back(loader(*_owner, name, buffer, key.size_bytes));

        if(shrink)
        {
            loaded_resources.back().free_underlying_buffer();
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
        RUNTIME_ERROR("Cannot open an already opened resource batch.");
    }

    this->file.open(file, std::ios::in | std::ios::binary);
}

size_t dib::resources::ResourceBatch::get_content_size(std::string_view filename) const
{
    auto key = resource_keys.find(filename);

    if(key == resource_keys.end())
        RUNTIME_ERROR("Attempt to get the content size of a non-existent resource.");
    
    return key->second.size_bytes;
}

void dib::resources::ResourceBatch::copy_content_to_buffer(std::string_view filename, void *buffer, bool) const
{
    auto key = resource_keys.find(filename);

    if(key == resource_keys.end())
        RUNTIME_ERROR("Attempt to get the content of a non-existent resource.");
    
    file.seekg(key->second.offset_bytes);
    file.read((char *) buffer, key->second.size_bytes);
    
    unsalt_buffer((char*) buffer, key->second.size_bytes, key->second.salt);
}

fs::path dib::resources::resource_batch_location()
{
    return dib::env::executable_directory_path() / ".dbatch";
}

// ResourceFolder //
const detail::LoadedResource &dib::resources::ResourceFolder::get_loaded(std::string_view name, Loader loader, size_t re_size, bool shrink, bool text)
{
    using namespace std::chrono;

    auto it = loaded_resources.find(name);
    bool should_reload = it == loaded_resources.end();

    if(it != loaded_resources.end())
    {
        auto now = resource_time_of_load[name];

        auto filename = folder / name;
        auto filetime = std::filesystem::last_write_time(filename).time_since_epoch();

        if(filetime > now)
        {
            should_reload = true;
            loaded_resources.erase(it);
        }
    }

    if(should_reload)
    {
        auto filename = folder / name;
        auto stream = text ? ifstream(filename) : ifstream(filename, ios::binary);

        if(stream.bad())
        {
            RUNTIME_ERROR("Attempt to open resource which does not exist.");
        }

        auto size = fs::file_size(filename);
        auto buffer = new char[size + re_size];

        stream.read(buffer + re_size, size);

        auto resource = loader(*_owner, name, buffer, size);

        name_storage.emplace_back(name);
        it = loaded_resources.insert({name_storage.back(), std::move(resource)}).first;

        if(shrink)
        {
            it->second.free_underlying_buffer();
        }

        auto sysclock = std::chrono::system_clock();
        resource_time_of_load[name_storage.back()] = std::chrono::duration_cast<std::chrono::milliseconds>(sysclock.now().time_since_epoch());
    }

    return it->second;
}

size_t dib::resources::ResourceFolder::get_content_size(std::string_view filename) const
{
    return fs::file_size(folder / filename);
}

void dib::resources::ResourceFolder::copy_content_to_buffer(std::string_view filename, void *buffer, bool text) const
{
    auto stream = text 
        ? ifstream(folder / filename) 
        : ifstream(folder / filename, ios::binary);

    if(stream.bad())
    {
        RUNTIME_ERROR("Attempt to get the content of a resource which does not exist.");
    }

    auto size = fs::file_size(filename);
    stream.read((char *) buffer, size);
}

// Resources //
void Resources::assert_initialized() const
{
    if(!_store.get())
        RUNTIME_ERROR("Attempt to use resources before initializing.");
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

void ResourceInterface<Text>::load(Resources &, Text &instance, std::string_view, const char *buffer, size_t size)
{
    instance = {buffer, size};
}

// Image //
void ResourceInterface<::Image>::load(Resources &, ::Image &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    instance = LoadImageFromMemory(ext_str.c_str(), (const unsigned char *)buffer, (int)size);
}

void ResourceInterface<::Image>::unload(Resources &, ::Image &instance)
{
    UnloadImage(instance);
}

// Texture //
void ResourceInterface<::Texture>::load(Resources &, ::Texture &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    auto img = LoadImageFromMemory(ext_str.c_str(), (const unsigned char *)buffer, (int)size);
    instance = LoadTextureFromImage(img);
    UnloadImage(img);
}

void ResourceInterface<::Texture>::unload(Resources &, ::Texture &instance)
{
    UnloadTexture(instance);
}

// Music //
void ResourceInterface<::Music>::load(Resources &, ::Music &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    instance = LoadMusicStreamFromMemory(ext_str.c_str(), (const unsigned char*)buffer, (int)size);
}

void ResourceInterface<::Music>::unload(Resources &, ::Music &instance)
{
    UnloadMusicStream(instance);
}

// Sound //
void ResourceInterface<::Sound>::load(Resources &, ::Sound &instance, std::string_view filename, const char *buffer, size_t size)
{
    std::string ext_str = std::filesystem::path{filename}.extension().string();
    auto wave = LoadWaveFromMemory(ext_str.c_str(), (const unsigned char*)buffer, (int)size);
    instance = LoadSoundFromWave(wave);
    UnloadWave(wave);
}

void ResourceInterface<::Sound>::unload(Resources &, ::Sound &instance)
{
    UnloadSound(instance);
}

// Model //
// TODO: develop a way to mark that a type does not need to be provided its file contents. Potentially provide helpers to make it simple for the implementation to do the allocation itself? Split the resource buffer in twain?
void ResourceInterface<::Model>::load(Resources &, ::Model &instance, std::string_view filename, const char *, size_t)
{
    auto virtual_filename = std::format("{}{}", raylib_resources, filename);
    instance = LoadModel(virtual_filename.c_str());
}

void ResourceInterface<::Model>::unload(Resources &, ::Model &instance)
{
    UnloadModel(instance);
}

// Shader //
void ResourceInterface<::Shader>::load(Resources &resources, ::Shader &instance, std::string_view filename, const char *buffer, size_t)
{
    std::istringstream sread(buffer);
	json::JsonReader jread(sread);

	std::string vertex, fragment;

	jread.read_start_object()
		.expect_kvp("vert", vertex)
		.expect_kvp("frag", fragment)
		.read_end_object()
		.end();

	auto base_path = fs::path(filename).parent_path();

	auto vert_code = resources.get<Text>(vertex);
	auto frag_code = resources.get<Text>(fragment);

	instance = LoadShaderFromMemory(frag_code->c_str(), vert_code->c_str());
}

void ResourceInterface<::Shader>::unload(Resources &, ::Shader &instance)
{
    UnloadShader(instance);
}

//* This code is preserved for later re-introduction *//
/*
void shaders::detail::process_shader(std::string &code, const fs::path &src, std::unordered_set<fs::path> &included)
{
	size_t index = 0;
	while (index = code.find("#include", index), index != std::string::npos)
	{
		auto str_begin = code.find('"', index);
		auto str_end = code.find('"', str_begin += 1);

		// TODO: HANDLE ERRORS HERE //

		auto str = code.substr(str_begin, str_end - str_begin);
		code.erase(index, str_end - index);

		auto file = fs::canonical(src.parent_path() / fs::path(std::move(str)));

		if (included.contains(file)) continue;

		auto filetext = files::read_all_text(file);
		included.insert(file);

		process_shader(filetext, file, included);

		str.insert(index, filetext);
		index += filetext.size();
	}
}
*/