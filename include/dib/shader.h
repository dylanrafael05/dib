#ifndef __DIB_SHADER_H
#define __DIB_SHADER_H

#include "dib/conststring.h"
#include "dib/resources.h"
#include "dib/types.h"
#include "dib/math/vec.h"
#include "raylib.h"

#include <unordered_map>
#include <filesystem>
#include <string>

namespace dib::shaders
{
	namespace detail
	{
		namespace fs = std::filesystem;

		// TODO: somehow find a way to 'preprocess' the shaders before building for release //
		void process_shader(std::string &code, const fs::path &src, std::unordered_set<fs::path> &included);

		void process_shader(std::string &code, const fs::path &src)
		{
			std::unordered_set<fs::path> included;
			process_shader(code, src, included);
		}
	}

	// NOTE: for now, only vertex/fragment pairs are supported for the Shader class
	using RawShader = ::Shader;

	class Shader
	{
		RawShader _shader;
		mutable std::unordered_map<std::string_view, int, strings::transparent_hash, std::equal_to<>> _locations;

		template<class Str>
		int get_location_inner(const Str &str) const
		{
			auto it = _locations.find(str);

			if (it == _locations.end())
			{
				_locations.insert({ (std::string_view)str, GetShaderLocation(_shader, ((std::string_view)str).data()) });
				it = _locations.find(str);
			}

			return it->second;
		}

	public:
		Shader() {}

		Shader(const Shader &) = delete;
		Shader(Shader &&) = delete;

		~Shader()
		{
			UnloadShader(_shader);
		}

		void load(std::string_view fragment_code, std::string_view vertex_code)
		{
			_shader = LoadShaderFromMemory(vertex_code.data(), fragment_code.data());
		}

		const RawShader &get() const { return _shader; }

		int get_param_location(const std::string_view &sv) const { return get_location_inner(sv); }
		template<strings::string_const Str>
		int get_param_location(strings::string_type<Str> s) const { return get_location_inner(s); }

		template<class T>
		void set_param(int location, const T &value) const
		{
			if constexpr (false);

			else if constexpr (std::is_same_v<T, int>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_INT);
			else if constexpr (std::is_same_v<T, math::int2>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_IVEC2);
			else if constexpr (std::is_same_v<T, math::int3>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_IVEC3);
			else if constexpr (std::is_same_v<T, math::int4>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_IVEC4);

			else if constexpr (std::is_same_v<T, float>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_FLOAT);
			else if constexpr (std::is_same_v<T, math::float2>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_VEC2);
			else if constexpr (std::is_same_v<T, math::float3>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_VEC3);
			else if constexpr (std::is_same_v<T, math::float4>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_VEC4);

			else if constexpr (std::is_same_v<T, Color>)
			{
				math::float4 col = { value.red / 255.f, value.green / 255.f, value.blue / 255.f, value.alpha / 255.f };
				SetShaderValue(_shader, location, &col, SHADER_UNIFORM_VEC4);
			}

			else if constexpr (std::is_same_v<T, Texture2D>) SetShaderValue(_shader, location, &value, SHADER_UNIFORM_SAMPLER2D);

			else static_assert(!std::is_same_v<T, T>, "Unknown shader parameter type.");
		}

		template<class T>
		void set_param(const std::string_view &key, const T &value) const
		{
			set_param(get_param_location(key), value);
		}

		template<class T, strings::string_const Str>
		void set_param(const strings::string_type<Str> &str, const T &value) const
		{
			set_param(get_param_location(str), value);
		}
	};
}

template<> struct dib::resources::ResourceInterface<dib::shaders::Shader>
{
	using Shader = dib::shaders::Shader;

	static void load(Shader &instance, std::string_view filename, const char *buffer, size_t size);

	static constexpr bool open_as_text = true;
	static constexpr bool shrink_on_construction = true;
};

#endif