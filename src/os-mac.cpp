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
		.download_help = "You can install cmake from https://cmake.org/download/ or by using Homebrew (`brew install cmake`) or MacPorts (`sudo port install cmake`).",
	},
	program_info{
		.name          = "git",
		.download_help = "You can install git from https://git-scm.com/download/mac or by using Homebrew (`brew install git`) or MacPorts (`sudo port install git`).",
	},
	program_info{
		.name          = "wget",
		.download_help = "You can install wget by using Homebrew (`brew install wget`) or MacPorts (`sudo port install wget`).",
	},
	program_info{
		.name          = "7z",
		.download_help = "You can install 7zip by using Homebrew (`brew install p7zip`) or MacPorts (`sudo port install p7zip`).",
	},
};

} // ---------------------------------------------------------------------------------------------------------

auto get_platform() -> platform {
	return platform::mac;
}

auto get_system_cache_dir() -> std::filesystem::path {
	if (const auto* home = std::getenv("HOME"); home && *home) {
		return std::filesystem::path{home} / "Library" / "Caches";
	}
	throw std::runtime_error("HOME could not be found");
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
