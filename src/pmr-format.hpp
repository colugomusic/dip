#pragma once

#include "context.hpp"

namespace dip {

template <typename... Args> [[nodiscard]]
auto pmr_format(const context* ctx, std::format_string<Args...> fmt, Args&&... args) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	std::format_to(std::back_inserter(str), fmt, std::forward<decltype(args)>(args)...);
	return str;
}

} // dip
