#include "mem.hpp"
#include "os.hpp"
#include <filesystem>
#include <vector>

namespace dip::os {

namespace { // -----------------------------------------------------------------------------------------------

auto is_executable(const std::filesystem::path& path) -> bool {
	std::error_code ec;
	const auto status = std::filesystem::status(path, ec);
	if (ec || !std::filesystem::is_regular_file(status)) {
		return false;
	}
	const auto perms = status.permissions();
	return (perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none
		|| (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none
		|| (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none;
}

} // ---------------------------------------------------------------------------------------------------------

auto enable_ansi_colors() -> void {
	// ANSI escapes are already supported by common Unix terminals and do not need an OS-specific enable step.
}

auto get_program_filename(std::string_view name) -> std::filesystem::path {
	return std::filesystem::path{name};
}

auto get_env_paths(mem_res* mem) -> std::pmr::vector<std::filesystem::path> {
	auto list = std::pmr::vector<std::filesystem::path>{mem};
	if (const auto* path_env = std::getenv("PATH")) {
		auto path_str = std::pmr::string{path_env, mem};
		auto pos = size_t{0};
		while (pos <= path_str.size()) {
			auto next = path_str.find(':', pos);
			if (next == std::pmr::string::npos) {
				next = path_str.size();
			}
			list.emplace_back(path_str.substr(pos, next - pos));
			if (next == path_str.size()) {
				break;
			}
			pos = next + 1;
		}
	}
	return list;
}

auto resolve_program_path(std::filesystem::path name, std::span<const std::filesystem::path> env_paths) -> std::optional<std::filesystem::path> {
	const auto cwd = std::filesystem::current_path();
	const auto direct = cwd / name;
	if (is_executable(direct)) {
		return direct;
	}
	for (const auto& p : env_paths) {
		const auto candidate = p.empty() ? name : p / name;
		if (is_executable(candidate)) {
			return candidate;
		}
	}
	return std::nullopt;
}

} // dip::os
