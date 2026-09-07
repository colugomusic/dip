#pragma once

#include <filesystem>
#include <variant>

namespace dip {

struct origin_git_repo           { std::pmr::string url; std::pmr::string commit; };
struct origin_git_tracked_branch { std::pmr::string url; std::pmr::string branch; std::pmr::string commit; };
struct origin_url                { std::pmr::string url; std::pmr::string md5; };

using origin = std::variant<std::filesystem::path, origin_git_repo, origin_git_tracked_branch, origin_url>;

struct dep_cmake_options {
	std::pmr::string any;
	std::pmr::string mac;
	std::pmr::string lin;
	std::pmr::string win;
};

struct dep {
	std::pmr::string name;
	dip::origin origin;
	dep_cmake_options cmake_options;
	std::pmr::string override_find_package_name;
};

} // dip
