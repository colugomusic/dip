#pragma once

#include "context.hpp"
#include "dep.hpp"
#include "dirs.hpp"
#include "pmr-format.hpp"
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
auto cmake_package_can_be_found(context* ctx, const dip::dirs& dirs, const prog_paths& progs, const dip::dep& dep) -> bool {
	const auto find_cmakelists = make_find_package_cmakelists(ctx, dep);
	// @TODO:
	return false;
}

} // dip
