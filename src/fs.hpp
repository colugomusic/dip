#pragma once

#include "context.hpp"
#include "pmr-format.hpp"
#include <array>
#include <filesystem>
#include <fstream>

namespace dip {

[[nodiscard]]
auto read_file_bytes(context* ctx, const std::filesystem::path& path) -> std::optional<std::pmr::vector<std::byte>> {
	auto file = std::ifstream{path, std::ios::binary};
	if (!file.is_open()) {
		ctx->log->info(pmr_format(ctx, "Failed to open file at '{}'", path.string()));
		return std::nullopt;
	}
	file.seekg(0, std::ios::end);
	const auto size = file.tellg();
	if (size < 0) {
		ctx->log->info(pmr_format(ctx, "Failed to determine size of file '{}'", path.string()));
		return std::nullopt;
	}
	file.seekg(0, std::ios::beg);
	auto bytes = std::pmr::vector<std::byte>{ctx->mem};
	bytes.resize(static_cast<std::size_t>(size));
	if (!bytes.empty()) {
		file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		if (!file) {
			ctx->log->info(pmr_format(ctx, "Failed to read file '{}'", path.string()));
			return std::nullopt;
		}
	}
	return bytes;
}

[[nodiscard]]
auto read_file_text(context* ctx, const std::filesystem::path& path) -> std::optional<std::pmr::string> {
	auto file = std::ifstream{path};
	if (!file.is_open()) {
		ctx->log->info(pmr_format(ctx, "Failed to open file at '{}'", path.string()));
		return std::nullopt;
	}
	return std::pmr::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}, ctx->mem};
}

auto write_text_to_file(const std::filesystem::path& path, std::string_view text) -> void {
	auto file = std::ofstream{path};
	if (!file.is_open()) {
		throw std::runtime_error{std::format("Failed to open file at '{}'", path.string())};
	}
	file << text.data();
}

[[nodiscard]]
// Finds the top-most matching file in the directory tree.
auto find_file_in_dir(context* ctx, const std::filesystem::path& dir, const std::filesystem::path& file_name) -> std::optional<std::filesystem::path> {
	ctx->log->detail(pmr_format(ctx, "Searching for '{}' in '{}'", file_name.string(), dir.string()));
	auto dirs_to_search = std::pmr::vector<std::filesystem::path>{{dir}, ctx->mem};
	while (!dirs_to_search.empty()) {
		auto more_dirs_to_search = std::pmr::vector<std::filesystem::path>{ctx->mem};
		for (const auto& dir : dirs_to_search) {
			if (!std::filesystem::exists(dir)) {
				continue;
			}
			for (const auto& entry : std::filesystem::directory_iterator{dir}) {
				if (entry.is_regular_file() && entry.path().filename() == file_name) {
					ctx->log->detail(pmr_format(ctx, "Found '{}' in '{}'", file_name.string(), dir.string()));
					return entry.path();
				}
				if (entry.is_directory()) {
					more_dirs_to_search.push_back(entry.path());
				}
			}
		}
		dirs_to_search = std::move(more_dirs_to_search);
	}
	return std::nullopt;
}

[[nodiscard]]
auto sanitize_to_folder_name(std::pmr::string text) -> std::filesystem::path {
	static constexpr auto DISALLOWED_CHARS = std::array{
		'<', '>', ':', '"', '/', '\\', '|', '?', '*'
	};
	for (auto& c : text) {
		if (std::ranges::find(DISALLOWED_CHARS, c) != std::cend(DISALLOWED_CHARS)) { c = '_'; }
		if (static_cast<unsigned char>(c) < 0x20) { c = '_'; }
	}
	while (!text.empty() && (text.back() == ' ' || text.back() == '.')) {
		text.pop_back();
	}
	return std::filesystem::path{text};
}

} // dip
