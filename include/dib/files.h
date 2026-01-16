#pragma once

#include <filesystem>
#include <string>

namespace dib::files
{
	using namespace std::filesystem;

	/// Read all text from the provided file
	std::string read_all_text(const path &file);
}