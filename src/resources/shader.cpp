#include "dib/shader.h"
#include "dib/json.h"
#include "dib/files.h"

#include <sstream>

using namespace dib;
namespace fs = std::filesystem;

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

void resources::ResourceInterface<shaders::Shader>::load(Shader &instance, std::string_view filename, const char *buffer, size_t size)
{
	std::istringstream sread(buffer, size);
	json::JsonReader jread(sread);

	std::string vertex, fragment;

	jread.read_start_object()
		.expect_kvp("vert", vertex)
		.expect_kvp("frag", fragment)
		.read_end_object()
		.end();

	auto base_path = fs::path(filename).parent_path();
	auto vert_path = fs::relative(fs::path(std::move(vertex)), base_path);
	auto frag_path = fs::relative(fs::path(std::move(fragment)), base_path);

	auto vert_code = files::read_all_text(vert_path);
	shaders::detail::process_shader(vert_code, vert_path);

	auto frag_code = files::read_all_text(frag_path);
	shaders::detail::process_shader(frag_code, frag_path);

	instance.load(frag_code, vert_code);
}