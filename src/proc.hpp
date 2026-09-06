#pragma once

#include "context.hpp"
#include "pmr-format.hpp"
#include <filesystem>
#include <ranges>
#include <tiny-process-library/process.hpp>

namespace dip {

[[nodiscard]]
auto make_proc_args(const std::filesystem::path& prog_path, std::string_view args) -> std::vector<std::string> {
	auto fn_subrange_to_string = [](auto&& subrange) { return std::string{subrange.begin(), subrange.end()}; };
	auto args_split            = args | std::views::split(' ') | std::views::transform(fn_subrange_to_string);
	auto out                   = std::vector<std::string>{prog_path.string()};
	std::ranges::copy(args_split, std::back_inserter(out));
	return out;
}

[[nodiscard]]
auto run_process_and_return_stderr(context* ctx, const std::filesystem::path& prog_path, std::string_view args) -> std::pmr::string {
	auto err = std::pmr::string{ctx->mem};
	auto read_stderr = [ctx, &err](const char* bytes, size_t n) {
		ctx->log->detail("read");
		err.append(bytes, n);
	};
	ctx->log->detail(pmr_format(ctx, "Running process: {} {}", prog_path.string(), args));
	TinyProcessLib::Process{make_proc_args(prog_path, args), "", nullptr, std::move(read_stderr)};
	ctx->log->detail(pmr_format(ctx, "stderr: {}", err));
	return err;
}

[[nodiscard]]
auto run_process_and_return_stdout(context* ctx, const std::filesystem::path& prog_path, std::string_view args) -> std::pmr::string {
	auto out = std::pmr::string{ctx->mem};
	auto read_stdout = [ctx, &out](const char* bytes, size_t n) {
		out.append(bytes, n);
	};
	ctx->log->detail(pmr_format(ctx, "Running process: {} {}", prog_path.string(), args));
	TinyProcessLib::Process{make_proc_args(prog_path, args), "", std::move(read_stdout)};
	ctx->log->detail(pmr_format(ctx, "stdout: {}", out));
	return out;
}

} // dip
