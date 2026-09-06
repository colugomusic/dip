#pragma once

#include "context.hpp"
#include "pmr-format.hpp"
#include <filesystem>
#include <fstream>

namespace dip {

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
