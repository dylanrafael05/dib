#ifndef __DIB_FILES_H
#define __DIB_FILES_H

#include <filesystem>
#include <string>

namespace dib::files
{
	using namespace std::filesystem;

	std::string read_all_text(const path &file);
}

#endif