#pragma once

#include "proc.hpp"
#include "progs.hpp"

namespace dip {

auto extract_to(context* ctx, const dip::prog_paths& progs, const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir_path) -> void {
	if (!std::filesystem::exists(dest_dir_path)) {
		std::filesystem::create_directories(dest_dir_path);
	}
	const auto args   = pmr_format(ctx, "x {} -y -o{}", archive_path.string(), dest_dir_path.string());
	const auto status = run_process_and_return_exit_status(ctx, progs.zip, args);
	if (status != 0) {
		throw std::runtime_error{std::format("Failed to extract archive '{}' to '{}'. Exit status: {}", archive_path.string(), dest_dir_path.string(), status)};
	}
}

} // dip
