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

} // dip
