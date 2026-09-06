#pragma once

#include "context.hpp"
#include <ranges>

namespace dip {

[[nodiscard]]
auto sort_and_remove_duplicates(context* ctx, std::ranges::random_access_range auto list) {
	using T = std::ranges::range_value_t<decltype(list)>;
	auto unique = std::pmr::vector<T>{ctx->mem};
	std::ranges::sort(list);
	std::ranges::unique_copy(list, std::back_inserter(unique));
	return unique;
}

} // dip
