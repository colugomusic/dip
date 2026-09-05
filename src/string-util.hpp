#pragma once

#include "context.hpp"

namespace dip {

[[nodiscard]]
auto to_string(context*, const std::pmr::string& v) -> std::pmr::string {
	return v;
}

[[nodiscard]]
auto to_string(context* ctx, const std::filesystem::path& v) -> std::pmr::string {
	return v.string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>(ctx->mem);
}

template <typename T> [[nodiscard]]
auto join(context* ctx, std::span<const T> items, std::string_view delimiter) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	for (size_t i = 0; i < items.size(); ++i) {
		str += to_string(ctx, items[i]);
		if (i < items.size() - 1) {
			str += delimiter;
		}
	}
	return str;
}

} // dip
