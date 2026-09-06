#pragma once

#include <filesystem>
#include <variant>

namespace dip {

struct origin_git_repo { std::pmr::string url; std::pmr::string tag; };
struct origin_url      { std::pmr::string url; std::pmr::string md5; };

using origin = std::variant<std::filesystem::path, origin_git_repo, origin_url>;

struct dep {
	std::pmr::string name;
	dip::origin origin;
	std::pmr::string cmake_options;
	std::pmr::string cmake_options_mac;
	std::pmr::string cmake_options_lin;
	std::pmr::string cmake_options_win;
	std::pmr::string override_find_package_name;
	std::filesystem::path registry_file;
	bool track = false;
};

} // dip
