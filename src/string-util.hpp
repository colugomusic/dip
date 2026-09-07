#pragma once

#include "context.hpp"
#include <span>

namespace dip {

[[nodiscard]]
auto to_string(context*, const std::pmr::string& v) -> std::pmr::string {
	return v;
}

[[nodiscard]]
auto to_string(context* ctx, const std::filesystem::path& v) -> std::pmr::string {
	return v.string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>(ctx->mem);
}

[[nodiscard]]
auto to_pmr_string(context* ctx, std::string_view v) -> std::pmr::string {
	return std::pmr::string{v.data(), v.size(), ctx->mem};
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

[[nodiscard]]
auto get_first_word(context* ctx, std::string_view str) -> std::pmr::string {
	auto fn_isspace = [](char c) { return std::isspace(static_cast<unsigned char>(c)); };
	const auto first_non_space = std::ranges::find_if_not(str, fn_isspace);
	const auto first_space     = std::ranges::find_if(first_non_space, str.end(), fn_isspace);
	return {first_non_space, first_space, ctx->mem};
}

[[nodiscard]]
auto trim(std::string_view v) -> std::string_view {
	const auto first = v.find_first_not_of(' ');
	if (first == std::string_view::npos) {
		return {};
	}
	const auto last = v.find_last_not_of(' ');
	return v.substr(first, last - first + 1);
}

[[nodiscard]]
auto split_csv(const context* ctx, std::string_view str) -> std::pmr::vector<std::pmr::string> {
	auto list  = std::pmr::vector<std::pmr::string>{ctx->mem};
	auto start = std::string_view::size_type{0};
	while (start < str.size()) {
		auto end = str.find(',', start);
		if (end == std::string_view::npos) {
			end = str.size();
		}
		if (const auto value_str = trim(str.substr(start, end - start)); !value_str.empty()) {
			list.emplace_back(value_str);
		}
		start = end + 1;
	}
	return list;
}

[[nodiscard]]
auto starts_with(std::string_view haystack, std::string_view needle) -> bool {
	return haystack.size() >= needle.size() && haystack.substr(0, needle.size()) == needle;
}

} // dip
