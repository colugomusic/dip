#pragma once

#include "cmake-options.hpp"
#include <filesystem>
#include <variant>

namespace dip {

struct origin_git_repo           { std::pmr::string url; std::pmr::string commit; };
struct origin_git_tracked_branch { std::pmr::string url; std::pmr::string branch; std::pmr::string commit; };
struct origin_url                { std::pmr::string url; std::pmr::string md5; };

using origin = std::variant<std::filesystem::path, origin_git_repo, origin_git_tracked_branch, origin_url>;

struct dep {
	std::pmr::string name;
	dip::origin origin;
	dip::cmake_options cmake_options;
};

} // dip
