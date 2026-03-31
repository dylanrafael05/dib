#pragma once

/// 

struct __DUMMY_TYPE_NAME_DO_NOT_USE_THIS_STRUCT;

#include <string_view>
#include <source_location>

namespace dib::types
{
	namespace detail
	{
		template<class T>
		consteval std::string_view type_name_helper()
		{
			return std::source_location::current().function_name();
		}

		constexpr std::string_view dummy_name = "__DUMMY_TYPE_NAME_DO_NOT_USE_THIS_STRUCT";
	}
	
	template<class T>
	constexpr std::string_view nameof = []
	{
		auto dummy_full = detail::type_name_helper<__DUMMY_TYPE_NAME_DO_NOT_USE_THIS_STRUCT>();

		auto dummy_begin = dummy_full.find(detail::dummy_name);
		auto dummy_end = dummy_begin + detail::dummy_name.length();

		auto dummy_begoff = dummy_begin;
		auto dummy_endoff = dummy_full.length() - dummy_end;

		auto full = detail::type_name_helper<T>();
		return full.substr(dummy_begoff, full.length() - dummy_endoff - dummy_begoff);
	}();
}