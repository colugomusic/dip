#pragma once

#include "context.hpp"
#include "fs.hpp"
#include <hash-library-md5.h>

namespace dip {

[[nodiscard]]
auto calc_md5(context* ctx, const std::filesystem::path& path) -> std::pmr::string {
	if (const auto bytes = read_file_bytes(ctx, path)) {
		const auto md5   = MD5{}(bytes->data(), bytes->size());
		return std::pmr::string{md5, ctx->mem};
	}
	throw std::runtime_error{std::format("Failed to calculate MD5 for file '{}' because we couldn't read it.", path.string())};
}

} // dip
