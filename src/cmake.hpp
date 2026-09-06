#pragma once

#include "context.hpp"
#include "dep.hpp"
#include "dirs.hpp"
#include "fs.hpp"
#include "pmr-format.hpp"
#include "proc.hpp"
#include "progs.hpp"

namespace dip {

[[nodiscard]]
auto make_find_package_cmakelists(context* ctx, const dip::dep& dep) -> std::pmr::string {
	const auto find_package_name = dep.override_find_package_name.empty() ? dep.name : dep.override_find_package_name;
	return pmr_format(ctx,
		"cmake_minimum_required(VERSION 3.30)\n"
		"project(dip-package-find-test CXX)\n"
		"find_package({} REQUIRED CONFIG NO_CMAKE_SYSTEM_PATH NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)\n",
		find_package_name
	);
}

[[nodiscard]]
auto cmake_package_can_be_found(context* ctx, const dip::dirs& dirs, const prog_paths& progs, const dip::dep& dep, std::string_view cfg) -> bool {
	const auto cmakelists_text      = make_find_package_cmakelists(ctx, dep);
	const auto pkg_check_dir_path   = make_pkg_check_dir_path(dirs);
	const auto install_dir_path     = make_install_dir_path(dirs, cfg);
	const auto cmakelists_path      = pkg_check_dir_path / "CMakeLists.txt";
	std::filesystem::create_directories(pkg_check_dir_path);
	write_text_to_file(cmakelists_path, cmakelists_text);
	const auto args = pmr_format(ctx, "-B {} -S {} -DCMAKE_PREFIX_PATH={}", pkg_check_dir_path.string(), pkg_check_dir_path.string(), install_dir_path.string());
	const auto found = run_process_and_return_exit_status(ctx, progs.cmake, args) == 0;
	if (!found) {
		ctx->log->detail(pmr_format(ctx, "CMake could not find package '{}'", dep.name));
	}
	return found;
}

} // dip
