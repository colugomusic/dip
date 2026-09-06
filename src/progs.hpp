#pragma once

#include <filesystem>

namespace dip {

struct prog_paths {
	std::filesystem::path git;
	std::filesystem::path wget;
};

} // dip
