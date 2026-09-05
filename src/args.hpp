#pragma once

#include "const-strings.hpp"
#include "context.hpp"
#include <argparse.hpp>

namespace dip {

struct arg_project_dir  { std::filesystem::path v; };
struct arg_track        { std::pmr::vector<std::pmr::string> v; };
struct arg_cache        { std::optional<std::filesystem::path> v; };
struct arg_root         { std::optional<std::filesystem::path> v; };
struct arg_install_self { bool v = false; };
struct arg_verbose      { bool v = false; };
struct arg_quiet        { bool v = false; };
struct arg_stfu         { bool v = false; };

static const auto ARG_TRACK_ALL = arg_track{ {ARG_TRACK_ALL_VALUE} };

struct args {
	arg_cache cache;
	arg_root root;
	arg_project_dir project_dir;
	arg_track track;
	arg_install_self install_self;
	arg_verbose verbose;
	arg_quiet quiet;
	arg_stfu stfu;
};

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
auto get_arg(const context*, arg_cache, const argparse::ArgumentParser& parser) -> arg_cache {
	if (parser.is_used(ARG_CACHE_NAME_LONG)) {
		return {parser.get<std::string>(ARG_CACHE_NAME_LONG)};
	}
	return {};
}

[[nodiscard]]
auto get_arg(const context*, arg_root, const argparse::ArgumentParser& parser) -> arg_root {
	if (parser.is_used(ARG_ROOT_NAME_LONG)) {
		return {parser.get<std::string>(ARG_ROOT_NAME_LONG)};
	}
	return {};
}

[[nodiscard]]
auto get_arg(const context*, arg_project_dir, const argparse::ArgumentParser& parser) -> arg_project_dir {
	return {parser.get<std::string>(ARG_PROJECT_NAME_LONG)};
}

[[nodiscard]]
auto get_arg(const context* ctx, arg_track, const argparse::ArgumentParser& parser) -> arg_track {
	if (parser.is_used(ARG_TRACK_NAME_LONG)) {
		const auto value = parser.get<std::string>(ARG_TRACK_NAME_LONG);
		if (value == ARG_TRACK_ALL_VALUE) { return ARG_TRACK_ALL; }
		else                              { return arg_track{ split_csv(ctx, value) }; }
	}
	return {};
}

[[nodiscard]]
auto get_arg(const context*, arg_quiet, const argparse::ArgumentParser& parser) -> arg_quiet {
    return {parser.get<bool>(ARG_QUIET_NAME_LONG)};
}

[[nodiscard]]
auto get_arg(const context*, arg_stfu, const argparse::ArgumentParser& parser) -> arg_stfu {
    return {parser.get<bool>(ARG_STFU_NAME_LONG)};
}

[[nodiscard]]
auto get_arg(const context*, arg_verbose, const argparse::ArgumentParser& parser) -> arg_verbose {
    return {parser.get<bool>(ARG_VERBOSE_NAME_LONG)};
}

[[nodiscard]]
auto get_args(const context* ctx, int argc, const char* argv[]) -> args {
	auto arg_parser = argparse::ArgumentParser{PROGRAM_NAME, PROGRAM_VERSION};
	arg_parser
		.add_argument(ARG_CACHE_NAME_SHORT, ARG_CACHE_NAME_LONG)
		.help(ARG_CACHE_HELP)
		;
	arg_parser
		.add_argument(ARG_ROOT_NAME_SHORT, ARG_ROOT_NAME_LONG)
		.help(ARG_ROOT_HELP)
		;
	arg_parser
		.add_argument(ARG_TRACK_NAME_SHORT, ARG_TRACK_NAME_LONG)
		.default_value(ARG_TRACK_ALL_VALUE)
		.help(ARG_TRACK_HELP)
		;
	arg_parser
		.add_argument(ARG_PROJECT_NAME_SHORT, ARG_PROJECT_NAME_LONG)
		.default_value(ctx->cwd.string())
		.help(ARG_PROJECT_HELP)
		;
	arg_parser
		.add_argument(ARG_QUIET_NAME_SHORT, ARG_QUIET_NAME_LONG)
		.default_value(false)
		.implicit_value(true)
		.help(ARG_QUIET_HELP)
		;
	arg_parser
		.add_argument(ARG_STFU_NAME_LONG)
		.default_value(false)
		.implicit_value(true)
		.help(ARG_STFU_HELP)
		;
	arg_parser
		.add_argument(ARG_VERBOSE_NAME_SHORT, ARG_VERBOSE_NAME_LONG)
		.default_value(false)
		.implicit_value(true)
		.help(ARG_VERBOSE_HELP)
		;
	arg_parser.parse_args(argc, argv);
	return dip::args{
		.cache       = get_arg(ctx, arg_cache{}, arg_parser),
		.root        = get_arg(ctx, arg_root{}, arg_parser),
		.project_dir = get_arg(ctx, arg_project_dir{}, arg_parser),
		.track       = get_arg(ctx, arg_track{}, arg_parser),
		.verbose     = get_arg(ctx, arg_verbose{}, arg_parser),
		.quiet       = get_arg(ctx, arg_quiet{}, arg_parser),
		.stfu        = get_arg(ctx, arg_stfu{}, arg_parser),
	};
}

} // dip
