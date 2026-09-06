#pragma once

#include "dep.hpp"
#include "fs.hpp"

namespace dip {

struct dirs {
	std::filesystem::path cache;
	std::filesystem::path root;
	std::filesystem::path project;
	std::filesystem::path dip;
};

[[nodiscard]]
auto make_pkg_check_dir_path(const dip::dirs& dirs) -> std::filesystem::path {
	return dirs.cache / "pkg-check";
}

[[nodiscard]]
auto make_install_dir_path(const dip::dirs& dirs, std::string_view cfg) -> std::filesystem::path {
	return dirs.root / "install" / cfg;
}

[[nodiscard]]
auto make_origin_dir_name(context* ctx, const origin_git_repo& v) -> std::filesystem::path {
	return sanitize_to_folder_name(v.url) / v.tag;
}

[[nodiscard]]
auto make_origin_dir_name(context* ctx, const origin_url& v) -> std::filesystem::path {
	return sanitize_to_folder_name(v.url);
}

[[nodiscard]]
auto make_origin_dir_name(context* ctx, const std::filesystem::path& v) -> std::filesystem::path {
	return sanitize_to_folder_name(to_string(ctx, v));
}

[[nodiscard]]
auto make_dir_name(context* ctx, const dip::origin& origin) -> std::filesystem::path {
	return std::visit([ctx](const auto& origin) { return make_origin_dir_name(ctx, origin); }, origin);
}

[[nodiscard]]
auto make_bld_dir_path(context* ctx, const dip::dirs& dirs, const dip::dep& dep) -> std::filesystem::path {
	return dirs.cache / dep.name / make_dir_name(ctx, dep.origin) / "bld";
}

[[nodiscard]]
auto make_dl_dir_path(context* ctx, const dip::dirs& dirs, const dip::dep& dep) -> std::filesystem::path {
	return dirs.cache / dep.name / make_dir_name(ctx, dep.origin) / "dl";
}

[[nodiscard]]
auto make_src_dir_path(context* ctx, const dip::dirs& dirs, const dip::dep& dep) -> std::filesystem::path {
	return dirs.cache / dep.name / make_dir_name(ctx, dep.origin) / "src";
}

} // dip
