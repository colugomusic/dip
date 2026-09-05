#pragma once

#include "logger.hpp"
#include "mem.hpp"
#include <filesystem>

namespace dip {

struct print_options {
	bool dep_tasks = true;
	bool errors    = true;
	bool warnings  = true;
	bool info      = true;
};

struct context {
	mem_res* mem = nullptr;
	logger* log  = nullptr;
	dip::print_options print_options;
	std::filesystem::path cwd;
	std::pmr::vector<std::filesystem::path> env_paths;
};

} // dip
