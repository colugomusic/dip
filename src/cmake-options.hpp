#pragma once

#include "context.hpp"
#include "list-util.hpp"
#include "os.hpp"
#include "string-util.hpp"
#include <string>
#include <vector>

namespace dip {

struct cmake_options {
	std::pmr::vector<std::pmr::string> any;
	std::pmr::vector<std::pmr::string> mac;
	std::pmr::vector<std::pmr::string> lin;
	std::pmr::vector<std::pmr::string> win;
};

[[nodiscard]]
auto get_cmake_options_list_mac(context* ctx, const cmake_options& project_options, const cmake_options& dep_options) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (!project_options.any.empty()) { std::ranges::copy(project_options.any, std::back_inserter(list)); }
	if (!project_options.mac.empty()) { std::ranges::copy(project_options.mac, std::back_inserter(list)); }
	if (!dep_options.any.empty())     { std::ranges::copy(dep_options.any, std::back_inserter(list)); }
	if (!dep_options.mac.empty())     { std::ranges::copy(dep_options.mac, std::back_inserter(list)); }
	return sort_and_remove_duplicates(ctx, std::move(list));
}

[[nodiscard]]
auto get_cmake_options_list_lin(context* ctx, const cmake_options& project_options, const cmake_options& dep_options) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (!project_options.any.empty()) { std::ranges::copy(project_options.any, std::back_inserter(list)); }
	if (!project_options.lin.empty()) { std::ranges::copy(project_options.lin, std::back_inserter(list)); }
	if (!dep_options.any.empty())     { std::ranges::copy(dep_options.any, std::back_inserter(list)); }
	if (!dep_options.lin.empty())     { std::ranges::copy(dep_options.lin, std::back_inserter(list)); }
	return sort_and_remove_duplicates(ctx, std::move(list));
}

[[nodiscard]]
auto get_cmake_options_list_win(context* ctx, const cmake_options& project_options, const cmake_options& dep_options) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (!project_options.any.empty()) { std::ranges::copy(project_options.any, std::back_inserter(list)); }
	if (!project_options.win.empty()) { std::ranges::copy(project_options.win, std::back_inserter(list)); }
	if (!dep_options.any.empty())     { std::ranges::copy(dep_options.any, std::back_inserter(list)); }
	if (!dep_options.win.empty())     { std::ranges::copy(dep_options.win, std::back_inserter(list)); }
	return sort_and_remove_duplicates(ctx, std::move(list));
}

[[nodiscard]]
auto get_cmake_options_list(context* ctx, os::platform platform, const cmake_options& project_options, const cmake_options& dep_options) -> std::pmr::vector<std::pmr::string> {
	switch (platform) {
		case os::platform::mac: { return get_cmake_options_list_mac(ctx, project_options, dep_options); }
		case os::platform::lin: { return get_cmake_options_list_lin(ctx, project_options, dep_options); }
		case os::platform::win: { return get_cmake_options_list_win(ctx, project_options, dep_options); }
		default:                { throw std::runtime_error{"Invalid platform."}; }
	}
}

[[nodiscard]]
auto get_cmake_options_string(context* ctx, os::platform platform, const cmake_options& project_options, const cmake_options& dep_options) -> std::pmr::string {
	const auto list = get_cmake_options_list(ctx, platform, project_options, dep_options);
	auto str = join<std::pmr::string>(ctx, list, " ");
	std::replace(str.begin(), str.end(), '\n', ' ');
	return str;
}

} // dip
