#pragma once

#include "progs.hpp"
#include "proc.hpp"

namespace dip {

[[nodiscard]]
auto make_wget_dl_args(context* ctx, std::string_view url, const std::filesystem::path& into_folder) -> std::pmr::string {
	return pmr_format(ctx, "-q -nc -P {} {}", into_folder.string(), url);
}

[[nodiscard]]
auto download_file(context* ctx, const prog_paths& progs, std::string_view url, const std::filesystem::path& into_folder) -> std::filesystem::path {
	const auto args   = make_wget_dl_args(ctx, url, into_folder);
	const auto status = run_process_and_return_exit_status(ctx, progs.wget, args);
	if (status != 0) {
		throw std::runtime_error{std::format("Failed to download file from '{}'. wget exited with status {}.", url, status)};
	}
	const auto file_path = into_folder / std::filesystem::path{url}.filename();
	if (!std::filesystem::exists(file_path)) {
		throw std::runtime_error{std::format("Expected downloaded file at '{}', but it does not exist.", file_path.string())};
	}
	return file_path;
}

} // dip
