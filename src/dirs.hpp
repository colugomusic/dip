#pragma once

#include "const-strings.hpp"
#include "context.hpp"
#include <filesystem>
#include <string_view>

namespace dip {

struct dirs {
	std::filesystem::path cache;
	std::filesystem::path root;
};

[[nodiscard]]
auto make_pkg_check_dir_path(const dip::dirs& dirs, std::string_view cfg) -> std::filesystem::path {
	return dirs.root / "pkg-check" / cfg;
}

[[nodiscard]]
auto make_install_path(const dip::dirs& dirs) -> std::filesystem::path {
	return dirs.root / "install";
}

[[nodiscard]]
auto make_install_prefix_path(const dip::dirs& dirs, std::string_view cfg) -> std::filesystem::path {
	return dirs.root / "install" / cfg;
}

[[nodiscard]]
auto make_install_meta_path(const dip::dirs& dirs, std::string_view cfg) -> std::filesystem::path {
	return dirs.root / "install" / cfg / "meta";
}

[[nodiscard]]
auto make_registry_override_path(const dip::dirs& dirs, std::string_view version) -> std::filesystem::path {
	return dirs.cache / version / FILENAME_REGISTRY_YML;
}

[[nodiscard]]
auto make_bld_dir_path(context*, const dip::dirs& dirs, std::string_view version, std::string_view cfg) -> std::filesystem::path {
	return dirs.cache / version / "bld" / cfg;
}

[[nodiscard]]
auto make_dl_dir_path(context*, const dip::dirs& dirs, std::string_view version) -> std::filesystem::path {
	return dirs.cache / version / "dl";
}

[[nodiscard]]
auto make_src_dir_path(context*, const dip::dirs& dirs, std::string_view version) -> std::filesystem::path {
	return dirs.cache / version / "src";
}

} // dip
