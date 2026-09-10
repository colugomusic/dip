#pragma once

#include "context.hpp"
#include "dirs.hpp"
#include "fs.hpp"
#include "pmr-format.hpp"
#include "proc.hpp"
#include "progs.hpp"

namespace dip {

[[nodiscard]]
auto make_find_package_cmakelists(context* ctx, std::string_view name) -> std::pmr::string {
	return pmr_format(ctx,
		"cmake_minimum_required(VERSION 3.30)\n"
		"project(dip-package-find-test CXX)\n"
		"find_package({} REQUIRED CONFIG NO_CMAKE_SYSTEM_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)\n",
		name
	);
}

[[nodiscard]]
auto cmake_package_can_be_found(context* ctx, const dip::dirs& dirs, const prog_paths& progs, std::string_view name, std::string_view cmake_config) -> bool {
	const auto cmakelists_text      = make_find_package_cmakelists(ctx, name);
	const auto pkg_check_dir_path   = make_pkg_check_dir_path(dirs, cmake_config);
	const auto install_prefix_path  = make_install_prefix_path(dirs, cmake_config);
	const auto cmakelists_path      = pkg_check_dir_path / "CMakeLists.txt";
	std::filesystem::create_directories(pkg_check_dir_path);
	write_text_to_file(cmakelists_path, cmakelists_text);
	const auto args = pmr_format(ctx, "-B {} -S {} -DCMAKE_PREFIX_PATH={}", pkg_check_dir_path.string(), pkg_check_dir_path.string(), install_prefix_path.string());
	const auto found = run_process_and_return_exit_status(ctx, progs.cmake, args) == 0;
	if (!found) {
		ctx->log->detail(pmr_format(ctx, "CMake could not find package '{}'", name));
	}
	return found;
}

auto cmake_configure(context* ctx, const prog_paths& progs, const std::filesystem::path& install_prefix, const std::filesystem::path& src_dir, const std::filesystem::path& bld_dir, std::string_view cmake_config, std::string_view cmake_options) -> void {
	const auto args = pmr_format(ctx,
		"-B {} "
		"-S {} "
		"--install-prefix {} "
		"-DCMAKE_PREFIX_PATH={} "
		"-DCMAKE_BUILD_TYPE={} "
		"{}",
		bld_dir.string(),
		src_dir.string(),
		install_prefix.string(),
		install_prefix.string(),
		cmake_config,
		cmake_options
	);
	const auto status = run_process_and_return_exit_status(ctx, progs.cmake, args);
	if (status != 0) {
		const auto err = std::format(
			"CMake configure failed.\n"
			"\tBuild dir: '{}'\n"
			"\tSource dir: '{}'\n"
			"\tInstall prefix: '{}'\n"
			"\tCMake config: '{}'\n"
			"\tCMake options: '{}'\n",
			bld_dir.string(),
			src_dir.string(),
			install_prefix.string(),
			cmake_config,
			cmake_options
		);
		throw std::runtime_error{err};
	}
}

auto cmake_build(context* ctx, const prog_paths& progs, const std::filesystem::path& bld_dir, std::string_view cmake_config) -> void {
	const auto args   = pmr_format(ctx, "--build {} --config {}", bld_dir.string(), cmake_config);
	const auto status = run_process_and_return_exit_status(ctx, progs.cmake, args);
	if (status != 0) {
		throw std::runtime_error{std::format("CMake build failed for build dir '{}' with config '{}'", bld_dir.string(), cmake_config)};
	}
}

auto cmake_install(context* ctx, const prog_paths& progs, const std::filesystem::path& bld_dir, std::string_view cmake_config) -> void {
	const auto args   = pmr_format(ctx, "--install {} --config {}", bld_dir.string(), cmake_config);
	const auto status = run_process_and_return_exit_status(ctx, progs.cmake, args);
	if (status != 0) {
		throw std::runtime_error{std::format("CMake install failed for build dir '{}' with config '{}'", bld_dir.string(), cmake_config)};
	}
}

} // dip
