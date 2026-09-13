#include "os.hpp"

namespace dip::os {

auto enable_ansi_colors() -> void {
	// @TODO: implement
}

auto get_platform() -> platform {
	// @TODO: implement
}

auto get_env_paths(mem_res* mem) -> std::pmr::vector<std::filesystem::path> {
	// @TODO: implement
}

auto get_program_filename(std::string_view name) -> std::filesystem::path {
	// @TODO: implement
}

auto resolve_program_path(std::filesystem::path name, std::span<const std::filesystem::path> env_paths) -> std::optional<std::filesystem::path> {
	// @TODO: implement
}

auto get_system_cache_dir() -> std::filesystem::path {
	// @TODO: implement
}

auto get_program_download_help(std::string_view name, mem_res* mem) -> std::pmr::string {
	// @TODO: implement
}

} // dip::os
