#include "os.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace dip::os {

namespace { // -----------------------------------------------------------------------------------------------

struct program_info {
	std::string_view name;
	std::string_view download_help;
};

const auto PROGRAM_INFO = std::array{
	program_info{
		.name          = "cmake",
		.download_help = "You can install cmake from https://cmake.org/download/ or by using your package manager (for example `sudo apt install cmake`, `sudo dnf install cmake`, or `sudo pacman -S cmake`).",
	},
	program_info{
		.name          = "git",
		.download_help = "You can install git from https://git-scm.com/downloads or by using your package manager (for example `sudo apt install git`, `sudo dnf install git`, or `sudo pacman -S git`).",
	},
	program_info{
		.name          = "wget",
		.download_help = "You can install wget from your package manager (for example `sudo apt install wget`, `sudo dnf install wget`, or `sudo pacman -S wget`).",
	},
	program_info{
		.name          = "7z",
		.download_help = "You can install 7zip from your package manager (for example `sudo apt install p7zip-full`, `sudo dnf install p7zip`, or `sudo pacman -S p7zip`).",
	},
};

} // ---------------------------------------------------------------------------------------------------------

auto get_platform() -> platform {
	return platform::lin;
}

auto get_system_cache_dir() -> std::filesystem::path {
	if (const auto* home = std::getenv("XDG_CACHE_HOME"); home && *home) {
		return std::filesystem::path{home};
	}
	if (const auto* home = std::getenv("HOME"); home && *home) {
		return std::filesystem::path{home} / ".cache";
	}
	throw std::runtime_error("Neither XDG_CACHE_HOME nor HOME could be found");
}

auto get_program_download_help(std::string_view name, mem_res* mem) -> std::pmr::string {
	const auto name_search = [name](const program_info& info) {
		return info.name == name;
	};
	if (const auto pos = std::ranges::find_if(PROGRAM_INFO, name_search); pos != std::end(PROGRAM_INFO)) {
		return std::pmr::string{pos->download_help, mem};
	}
	return std::pmr::string{mem};
}

} // dip::os
