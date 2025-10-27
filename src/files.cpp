#include "dib/files.h"

#include <iostream>
#include <fstream>
#include <sstream>

std::string dib::files::read_all_text(const path &file)
{
	std::ifstream stream(file);
	std::stringstream strstream;

	strstream << stream.rdbuf();

	return std::move(strstream).str();
}