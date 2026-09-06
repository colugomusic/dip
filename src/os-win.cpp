#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "os.hpp"
#include <array>
#include <cstdlib>
#include <string>
#include <string_view>
#include <tiny-process-library/process.hpp>
#include <Windows.h>
#include <winerror.h>
#include <stringapiset.h>
#include <shlobj.h>

namespace dip::os {

struct program_info {
	std::string_view name;
	std::string_view download_help;
};

static const auto PROGRAM_INFO = std::array{
	program_info{
		.name          = "cmake",
		.download_help = "You can download git for Windows from https://cmake.org/download/ or by installing a package manager like Chocolatey (https://chocolatey.org/) and running `choco install cmake`.",
	},
	program_info{
		.name          = "git",
		.download_help = "You can download git for Windows from https://git-scm.com/download/win or by installing a package manager like Chocolatey (https://chocolatey.org/) and running `choco install git`.",
	},
	program_info{
		.name          = "wget",
		.download_help = "You can download wget for Windows from https://eternallybored.org/misc/wget/ or by installing a package manager like Chocolatey (https://chocolatey.org/) and running `choco install wget`.",
	},
	program_info{
		.name          = "7-zip",
		.download_help = "You can download 7-zip for Windows from https://www.7-zip.org/download.html or by installing a package manager like Chocolatey (https://chocolatey.org/) and running `choco install 7zip`.",
	},
};

[[nodiscard]] static
auto shorten(std::wstring_view wide) -> std::string {
	if (wide.empty()) {
		return {};
	}
	int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), NULL, 0, NULL, NULL);
	auto buf = std::string{};
	buf.resize(n);
	WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), &buf[0], n, NULL, NULL);
	return buf;
}

[[nodiscard]] static
auto get_known_folder(REFKNOWNFOLDERID folder_id) -> std::optional<std::filesystem::path> {
	struct scope_co_free {
		LPWSTR pointer = NULL;
		scope_co_free(LPWSTR pointer) : pointer(pointer) {};
		~scope_co_free() { CoTaskMemFree(pointer); }
	};
	auto wsz_path = LPWSTR{NULL};
	const auto hr = SHGetKnownFolderPath(folder_id, KF_FLAG_CREATE, NULL, &wsz_path);
	auto mem = scope_co_free{wsz_path};
	if (!SUCCEEDED(hr)) {
		return std::nullopt;
	}
	return shorten(wsz_path);
}

[[nodiscard]] static
auto is_executable(const std::filesystem::path& path) -> bool {
	return std::filesystem::is_regular_file(path) && path.extension() == ".exe";
}

auto get_env_paths(mem_res* mem) -> std::pmr::vector<std::filesystem::path> {
	auto list = std::pmr::vector<std::filesystem::path>{mem};
	auto length = size_t{0};
	if (getenv_s(&length, nullptr, 0, "PATH") == 0 && length > 0) {
		auto path_str = std::pmr::string(length, '\0', mem);
		if (getenv_s(&length, path_str.data(), path_str.size(), "PATH") == 0) {
			path_str.resize(length - 1);
			auto pos = size_t{0};
			while (pos < path_str.size()) {
				auto next = path_str.find(';', pos);
				if (next == std::pmr::string::npos) {
					next = path_str.size();
				}
				list.emplace_back(path_str.substr(pos, next - pos));
				pos = next + 1;
			}
		}
	}
	return list;
}

auto get_system_cache_dir() -> std::filesystem::path {
	if (const auto dir = get_known_folder(FOLDERID_LocalAppData)) {
		return *dir;
	}
	throw std::runtime_error("LocalAppData could not be found");
}

auto enable_ansi_colors() -> void {
	const auto h = GetStdHandle(STD_OUTPUT_HANDLE);
	auto mode = DWORD{0};
	GetConsoleMode(h, &mode);
	SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

auto get_program_filename(std::string_view name) -> std::filesystem::path {
	return std::filesystem::path{name}.replace_extension("exe");
}

auto resolve_program_path(std::filesystem::path name, std::span<const std::filesystem::path> env_paths) -> std::optional<std::filesystem::path> {
	name.replace_extension("exe");
	const auto cwd = std::filesystem::current_path();
	if (is_executable(cwd / name)) {
		return cwd / name;
	}
	for (const auto& p : env_paths) {
		if (is_executable(p / name)) {
			return p / name;
		}
	}
	return std::nullopt;
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
