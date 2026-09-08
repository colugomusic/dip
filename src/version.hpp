#pragma once

#include "dep.hpp"
#include "md5.hpp"
#include "string-util.hpp"
#include "yaml.hpp"
#include <span>

namespace dip {

[[nodiscard]]
auto to_bytes(context* ctx, std::span<const std::pmr::string> v) -> std::pmr::vector<std::byte> {
	auto bytes = std::pmr::vector<std::byte>{ctx->mem};
	for (const auto& str : v) {
		const auto str_bytes = std::as_bytes(std::span{str.data(), str.size()});
		std::ranges::copy(str_bytes, std::back_inserter(bytes));
	}
	return bytes;
}

[[nodiscard]]
auto to_bytes(context* ctx, const std::filesystem::path& v) -> std::pmr::vector<std::byte> {
	const auto str_bytes = std::as_bytes(std::span{v.string().data(), v.string().size()});
	return std::pmr::vector<std::byte>{str_bytes.begin(), str_bytes.end(), ctx->mem};
}

[[nodiscard]]
auto to_bytes(context* ctx, const origin_git_repo& v) -> std::pmr::vector<std::byte> {
	auto bytes = std::pmr::vector<std::byte>{ctx->mem};
	const auto url_bytes    = to_bytes(ctx, v.url);
	const auto commit_bytes = to_bytes(ctx, v.commit);
	std::ranges::copy(url_bytes, std::back_inserter(bytes));
	std::ranges::copy(commit_bytes, std::back_inserter(bytes));
	return bytes;
}

[[nodiscard]]
auto to_bytes(context* ctx, const origin_git_tracked_branch& v) -> std::pmr::vector<std::byte> {
	auto bytes = std::pmr::vector<std::byte>{ctx->mem};
	const auto url_bytes    = to_bytes(ctx, v.url);
	const auto branch_bytes = to_bytes(ctx, v.branch);
	std::ranges::copy(url_bytes, std::back_inserter(bytes));
	std::ranges::copy(branch_bytes, std::back_inserter(bytes));
	return bytes;
}

[[nodiscard]]
auto to_bytes(context* ctx, const origin_url& v) -> std::pmr::vector<std::byte> {
	auto bytes = std::pmr::vector<std::byte>{ctx->mem};
	const auto url_bytes = to_bytes(ctx, v.url);
	std::ranges::copy(url_bytes, std::back_inserter(bytes));
	return bytes;
}

[[nodiscard]]
auto to_bytes(context* ctx, const dip::origin& origin) -> std::pmr::vector<std::byte> {
	return std::visit([ctx](const auto& origin) { return to_bytes(ctx, to_bytes(ctx, origin)); }, origin);
}

[[nodiscard]]
auto make_version_string(context* ctx, const dip::origin& origin, std::span<const std::pmr::string> cmake_options_list) -> std::pmr::string {
	const auto options_bytes = to_bytes(ctx, cmake_options_list);
	const auto origin_bytes  = to_bytes(ctx, origin);
	auto md5                 = MD5{};
	md5.add(options_bytes.data(), options_bytes.size());
	md5.add(origin_bytes.data(), origin_bytes.size());
	const auto hash = md5.getHash();
	return std::pmr::string{hash, ctx->mem};
}

} // dip
