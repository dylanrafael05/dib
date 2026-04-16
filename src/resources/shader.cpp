#include "dib/resources/shader.h"
#include "dib/resources/resources.h"
#include <filesystem>

using namespace dib::res;
namespace fs = std::filesystem;

::Shader Shader2D::get() const 
{
    return _shader;
}

std::string preprocess_shader(dib::res::Resources &resources, std::string_view text, std::vector<ResourceHandle<Text>> &deps);

/// Load a shader from its defining json file
void ResourceInterface<Shader2D>::load(Resources &resources, Shader2D &instance, std::string_view filename, const char *buffer, size_t)
{
    std::istringstream sread(buffer);
	json::JsonReader jread(sread);

	dib::option::Option<std::string> vert, frag;

	jread.read_start_object()
		.expect_kvp("vert", vert)
		.expect_kvp("frag", frag)
		.read_end_object()
		.end();

	auto base_path = fs::path(filename).parent_path();

    instance._frag = MOVE(frag).map([&](auto &&v) { return resources.get<Text>(v); });
    instance._vert = MOVE(vert).map([&](auto &&v) { return resources.get<Text>(v); });

	auto vert_code = instance._vert.ref().map([&](auto &&v) { return preprocess_shader(resources, v->str(), instance._includes); });
	auto frag_code = instance._frag.ref().map([&](auto &&v) { return preprocess_shader(resources, v->str(), instance._includes); });

	instance._shader = LoadShaderFromMemory(
        vert_code.ref().map(&std::string::c_str).unwrap_or(nullptr), 
        frag_code.ref().map(&std::string::c_str).unwrap_or(nullptr));

    if(!IsShaderValid(instance._shader))
    {
        LOGF("Could not load shader {}, defaulting to standard shader.", filename);
        instance._shader = LoadShader(0, 0);
    }
}

/// Delegate to Raylib to unload our shader
void ResourceInterface<Shader2D>::unload(Resources &, Shader2D &instance)
{
    UnloadShader(instance.get());
}

/// Get the depedencies of the provided Shader2D resource
void ResourceInterface<Shader2D>::get_dependencies(Shader2D &instance, std::vector<std::string> &deps)
{
    if(instance._frag)
        deps.emplace_back(instance._frag.unwrap().name());

    if(instance._vert)
        deps.emplace_back(instance._vert.unwrap().name());

    for(auto &include : instance._includes)
        deps.emplace_back(include.name());
}

/// Apply custom preprocessing to the provided shader 
std::string preprocess_shader(dib::res::Resources &resources, std::string_view text_v, std::vector<ResourceHandle<Text>> &deps)
{
    constexpr std::string_view include_header = "@include";
    constexpr std::string_view whitespace = " \t";

    std::string text(text_v);
    size_t includepos;

    // Repeat until all include headers have been resolved //
    while((includepos = text.find(include_header)) != std::string::npos)
    {
        // Find the newline (or end of string) that ends the @include //
        auto eol_or_end = text.find("\n", includepos);
        if(eol_or_end == std::string::npos)
        {
            LOGF("Missing newline after {}", include_header);
            eol_or_end = text.length();
        }

        // Find the name of the resource to include //
        auto filename_start = includepos + include_header.length();
        filename_start = text.find_first_not_of(whitespace, filename_start);

        auto filename_end = eol_or_end;
        filename_end = text.find_last_not_of(whitespace, filename_end);

        auto view = std::string_view(text);
        auto filename = view.substr(filename_start, filename_end - filename_start);

        // Get a handle to the included text //
        auto file_text = resources.get<dib::res::Text>(filename);
        deps.emplace_back(file_text);

        // Include the text //
        text = std::format("{}\n/***** begin {} *****/\n{}\n/***** end {} *****/\n{}", 
            view.substr(0, includepos), 
            filename,
            file_text->str(), 
            filename,
            view.substr(eol_or_end));
    }

    return text;
}