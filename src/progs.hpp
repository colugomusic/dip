#pragma once

#include <filesystem>

namespace dip {

struct prog_paths {
	std::filesystem::path cmake;
	std::filesystem::path git;
	std::filesystem::path wget;
	std::filesystem::path zip;
};

} // dip
