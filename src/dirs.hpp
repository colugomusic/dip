#pragma once

#include <filesystem>

namespace dip {

struct dirs {
	std::filesystem::path cache;
	std::filesystem::path root;
	std::filesystem::path project;
	std::filesystem::path dip;
};

} // dip
