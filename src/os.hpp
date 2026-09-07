#pragma once

#include "mem.hpp"
#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

namespace dip::os {

enum class platform { mac, lin, win };

auto enable_ansi_colors() -> void;
auto get_platform() -> platform;
[[nodiscard]] auto get_env_paths(mem_res* mem) -> std::pmr::vector<std::filesystem::path>;
[[nodiscard]] auto get_program_filename(std::string_view name) -> std::filesystem::path;
[[nodiscard]] auto resolve_program_path(std::filesystem::path name, std::span<const std::filesystem::path> env_paths) -> std::optional<std::filesystem::path>;
[[nodiscard]] auto get_system_cache_dir() -> std::filesystem::path;
[[nodiscard]] auto get_program_download_help(std::string_view name, mem_res* mem) -> std::pmr::string;

} // dip::os
