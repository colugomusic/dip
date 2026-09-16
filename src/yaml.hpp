#pragma once

#include "cmake-options.hpp"
#include "const-strings.hpp"
#include "context.hpp"
#include "dep.hpp"
#include "fs.hpp"
#include "pmr-format.hpp"
#include "string-util.hpp"
#include <fkYAML/node.hpp>

namespace dip {

using node_t = fkyaml::basic_node<std::vector, fkyaml::ordered_map>;

struct yml_project_settings {
	std::pmr::string name;
	std::pmr::vector<std::pmr::string> package_names;
	dip::cmake_options cmake_options;
	std::pmr::vector<std::pmr::string> cmake_configs;
};

struct yml_registry {
	std::pmr::vector<dip::dep> deps;
};

struct yml_meta {
	std::pmr::string version;
};

[[nodiscard]]
auto fn_dep_name_is(std::string_view name) {
	return [name](const dip::dep& dep) { return dep.name == name; };
}

[[nodiscard]]
auto get_dep(yml_registry* registry, std::string_view name) -> dip::dep* {
	if (auto pos = std::ranges::find_if(registry->deps, fn_dep_name_is(name)); pos != std::cend(registry->deps)) {
		return &*pos;
	}
	throw std::runtime_error(std::format("Dependency '{}' not found in registry.", name));
}

[[nodiscard]]
auto get_dep(const yml_registry& registry, std::string_view name) -> const dip::dep& {
	if (auto pos = std::ranges::find_if(registry.deps, fn_dep_name_is(name)); pos != std::cend(registry.deps)) {
		return *pos;
	}
	throw std::runtime_error(std::format("Dependency '{}' not found in registry.", name));
}

[[nodiscard]]
auto read_string(context* ctx, const node_t& node, std::string_view key) -> std::optional<std::pmr::string> {
	if (node.contains(key)) {
		const auto value_node = node.at(key);
		if (value_node.is_string()) {
			const auto str = value_node.get_value<std::string>();
			return std::pmr::string{str.data(), str.size(), ctx->mem};
		}
	}
	return std::nullopt;
}

[[nodiscard]]
auto to_pmr_string(context* ctx, const node_t& mapping, std::string_view key) -> std::pmr::string {
	auto node = mapping.at(key);
	if (!node.is_string()) {
		throw std::runtime_error{std::format("The '{}' key must be a string, but found '{}'.", key, fkyaml::to_string(node.get_type()))};
	}
	auto str = node.get_value<std::string>();
	return {str.data(), str.size(), ctx->mem};
}

[[nodiscard]]
auto find_origin_git_repo(context* ctx, const node_t& mapping) -> std::optional<dip::origin_git_repo> {
	if (mapping.contains(KEY_GIT)) {
		auto url = to_pmr_string(ctx, mapping, KEY_GIT);
		if (mapping.contains(KEY_COMMIT)) {
			auto commit = to_pmr_string(ctx, mapping, KEY_COMMIT);
			return origin_git_repo{
				.url    = url,
				.commit = commit
			};
		}
	}
	return std::nullopt;
}

[[nodiscard]]
auto find_origin_git_tracked_branch(context* ctx, const node_t& mapping) -> std::optional<dip::origin_git_tracked_branch> {
	if (mapping.contains(KEY_GIT)) {
		auto url = to_pmr_string(ctx, mapping, KEY_GIT);
		if (mapping.contains(KEY_TRACK)) {
			auto branch = to_pmr_string(ctx, mapping, KEY_TRACK);
			if (mapping.contains(KEY_COMMIT)) {
				auto commit = to_pmr_string(ctx, mapping, KEY_COMMIT);
				return origin_git_tracked_branch{
					.url    = url,
					.branch = branch,
					.commit = commit
				};
			}
			return origin_git_tracked_branch{
				.url    = url,
				.branch = branch
			};
		}
	}
	return std::nullopt;
}

[[nodiscard]]
auto find_origin_url(context* ctx, const node_t& mapping) -> std::optional<dip::origin_url> {
	if (mapping.contains(KEY_URL)) {
		auto url = dip::origin_url{
			.url = to_pmr_string(ctx, mapping, KEY_URL),
		};
		if (mapping.contains(KEY_MD5)) {
			url.md5 = to_pmr_string(ctx, mapping, KEY_MD5);
		}
		return url;
	}
	return std::nullopt;
}

[[nodiscard]]
auto find_origin(context* ctx, const node_t& mapping) -> dip::origin {
	if (const auto url                = find_origin_url(ctx, mapping))                { return *url; }
	if (const auto git_tracked_branch = find_origin_git_tracked_branch(ctx, mapping)) { return *git_tracked_branch; }
	if (const auto git_repo           = find_origin_git_repo(ctx, mapping))           { return *git_repo; }
	if (mapping.contains(KEY_GIT)) {
		throw std::runtime_error{std::format("The '{}' key must be accompanied by either a '{}' or '{}' key.", KEY_GIT, KEY_COMMIT, KEY_TRACK)};
	}
	throw std::runtime_error{std::format("Each item in the registry must contain either a '{}' or '{}' key.", KEY_URL, KEY_GIT)};
}

[[nodiscard]]
auto find_string(context* ctx, const node_t& mapping, std::string_view key) -> std::optional<std::pmr::string> {
	if (mapping.contains(key)) {
		return to_pmr_string(ctx, mapping, key);
	}
	return std::nullopt;
}

[[nodiscard]]
auto find_bool(context*, const node_t& mapping, std::string_view key) -> std::optional<bool> {
	if (mapping.contains(key)) {
		auto node = mapping.at(key);
		if (!node.is_boolean()) {
			throw std::runtime_error{std::format("The '{}' key must be a boolean, but found '{}'.", key, fkyaml::to_string(node.get_type()))};
		}
		return node.get_value<bool>();
	}
	return std::nullopt;
}

[[nodiscard]]
auto find_cmake_config_list(context* ctx, const node_t& root) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (root.contains(KEY_CMAKE_CONFIGS)) {
		const auto cfgs_node = root.at(KEY_CMAKE_CONFIGS);
		if (!cfgs_node.is_sequence()) {
			throw std::runtime_error{std::format("The '{}' key must be a sequence, but found '{}'.", KEY_CMAKE_CONFIGS, fkyaml::to_string(cfgs_node.get_type()))};
		}
		for (const auto& cfg_node : cfgs_node) {
			if (!cfg_node.is_string()) {
				throw std::runtime_error{std::format("Each item in the '{}' sequence must be a string, but found '{}'.", KEY_CMAKE_CONFIGS, fkyaml::to_string(cfg_node.get_type()))};
			}
			const auto cfg_str = cfg_node.get_value<std::string>();
			list.push_back(std::pmr::string{cfg_str.data(), cfg_str.size(), ctx->mem});
		}
		return list;
	}
	ctx->log->info(pmr_format(ctx, "No '{}' key was found in project settings, so you must specify the cmake configs on the command line, e.g. `--cfg Debug,RelWithDebInfo`", KEY_CMAKE_CONFIGS));
	return list;
}

[[nodiscard]]
auto find_cmake_options(context* ctx, const node_t& mapping) -> cmake_options {
	auto fn_str_to_list         = fn_split_by_whitespace(ctx);
	auto empty_list             = std::pmr::vector<std::pmr::string>{ctx->mem};
	auto cmake_options_any_str  = find_string(ctx, mapping, KEY_CMAKE_OPTIONS);
	auto cmake_options_mac_str  = find_string(ctx, mapping, KEY_CMAKE_OPTIONS_MAC);
	auto cmake_options_lin_str  = find_string(ctx, mapping, KEY_CMAKE_OPTIONS_LIN);
	auto cmake_options_win_str  = find_string(ctx, mapping, KEY_CMAKE_OPTIONS_WIN);
	auto cmake_options_any_list = cmake_options_any_str.transform(fn_str_to_list).value_or(empty_list);
	auto cmake_options_mac_list = cmake_options_mac_str.transform(fn_str_to_list).value_or(empty_list);
	auto cmake_options_lin_list = cmake_options_lin_str.transform(fn_str_to_list).value_or(empty_list);
	auto cmake_options_win_list = cmake_options_win_str.transform(fn_str_to_list).value_or(empty_list);
	return {
		.any = cmake_options_any_list,
		.mac = cmake_options_mac_list,
		.lin = cmake_options_lin_list,
		.win = cmake_options_win_list,
	};
}

[[nodiscard]]
auto get_package_names(context* ctx, const node_t& mapping) -> std::pmr::vector<std::pmr::string> {
	auto package_names = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (mapping.contains(KEY_PACKAGE_NAMES)) {
		const auto pkg_names_node = mapping.at(KEY_PACKAGE_NAMES);
		if (!pkg_names_node.is_sequence()) {
			throw std::runtime_error{std::format("The '{}' key must be a sequence, but found '{}'.", KEY_PACKAGE_NAMES, fkyaml::to_string(pkg_names_node.get_type()))};
		}
		for (const auto& pkg_name_node : pkg_names_node) {
			if (!pkg_name_node.is_string()) {
				throw std::runtime_error{std::format("Each item in the '{}' sequence must be a string, but found '{}'.", KEY_PACKAGE_NAMES, fkyaml::to_string(pkg_name_node.get_type()))};
			}
			package_names.push_back(to_pmr_string(ctx, pkg_name_node.get_value<std::string>()));
		}
	}
	return package_names;
}

[[nodiscard]]
auto read_project_settings_yml(context* ctx, const std::filesystem::path& dip_dir) -> yml_project_settings {
	const auto path = dip_dir / FILENAME_SETTINGS_YML;
	if (std::filesystem::exists(path)) {
		ctx->log->detail(pmr_format(ctx, "Reading project settings from '{}'", path.string()));
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = node_t::deserialize(*text);
			const auto name = find_string(ctx, node, KEY_NAME);
			if (!name) {
				throw std::runtime_error{std::format("The '{}' key is required in project settings, but was not found in '{}'.", KEY_NAME, path.string())};
			}
			return yml_project_settings {
				.name          = *name,
				.package_names = get_package_names(ctx, node),
				.cmake_options = find_cmake_options(ctx, node),
				.cmake_configs = find_cmake_config_list(ctx, node)
			};
		}
		throw std::runtime_error{std::format("Failed to read project settings from '{}'", path.string())};
	}
	throw std::runtime_error{std::format("No project settings file found at '{}'", path.string())};
}

[[nodiscard]]
auto read_dep_yml(context* ctx, const node_t& mapping) -> dip::dep {
	if (!mapping.is_mapping())       { throw std::runtime_error{std::format("Each item in the registry must be a mapping, but found '{}'.", fkyaml::to_string(mapping.get_type()))}; }
	if (!mapping.contains(KEY_NAME)) { throw std::runtime_error{std::format("Each item in the registry must contain a '{}' key.", KEY_NAME)}; }
	auto name = to_pmr_string(ctx, mapping, KEY_NAME);
	try {
		auto origin        = find_origin(ctx, mapping);
		auto package_names = get_package_names(ctx, mapping);
		return dip::dep {
			.name          = std::move(name),
			.package_names = std::move(package_names),
			.origin        = std::move(origin),
			.cmake_options = find_cmake_options(ctx, mapping),
		};
	}
	catch (const std::exception& err) { throw std::runtime_error{std::format("Error reading dependency '{}' from registry: {}", name, err.what())}; }
	catch (...)                       { throw std::runtime_error{std::format("Unknown error reading dependency '{}' from registry.", name)}; }
}

[[nodiscard]]
auto read_deps_yml(context* ctx, const node_t& list) -> std::pmr::vector<dip::dep> {
	auto deps = std::pmr::vector<dip::dep>{ctx->mem};
	if (list.is_sequence()) {
		for (const auto& node : list) {
			deps.push_back(read_dep_yml(ctx, node));
		}
	}
	return deps;
};

[[nodiscard]]
auto read_registry_yml(context* ctx, const std::filesystem::path& path) -> yml_registry {
	if (std::filesystem::exists(path)) {
		ctx->log->detail(pmr_format(ctx, "Reading registry from '{}'", path.string()));
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = node_t::deserialize(*text);
			return yml_registry{
				.deps = read_deps_yml(ctx, node)
			};
		}
		ctx->log->info(pmr_format(ctx, "Failed to read registry from '{}'", path.string()));
		return {};
	}
	ctx->log->info(pmr_format(ctx, "No registry file found at '{}'", path.string()));
	return {};
}

[[nodiscard]]
auto read_meta_yml(context* ctx, const std::filesystem::path& path) -> yml_meta {
	if (std::filesystem::exists(path)) {
		ctx->log->detail(pmr_format(ctx, "Reading meta info from '{}'", path.string()));
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = node_t::deserialize(*text);
			if (!node.is_mapping())          { throw std::runtime_error{std::format("The meta file must be a mapping, but found '{}'.", fkyaml::to_string(node.get_type()))}; }
			if (!node.contains(KEY_VERSION)) { throw std::runtime_error{std::format("The meta file must contain a '{}' key.", KEY_VERSION)}; }
			return yml_meta{
				.version = to_pmr_string(ctx, node, KEY_VERSION)
			};
		}
		throw std::runtime_error{std::format("Failed to read meta info from '{}'", path.string())};
	}
	ctx->log->detail(pmr_format(ctx, "No meta file found at '{}'", path.string()));
	return {};
}

auto map_origin_into(node_t::mapping_type* mapping, const std::filesystem::path& origin) -> void {
	(*mapping)[KEY_PATH] = origin.string();
}

auto map_origin_into(node_t::mapping_type* mapping, const origin_git_repo& origin) -> void {
	(*mapping)[KEY_GIT] = origin.url;
	if (!origin.commit.empty()) {
		(*mapping)[KEY_COMMIT] = origin.commit;
	}
}

auto map_origin_into(node_t::mapping_type* mapping, const origin_git_tracked_branch& origin) -> void {
	(*mapping)[KEY_GIT] = origin.url;
	if (!origin.branch.empty()) { (*mapping)[KEY_TRACK]  = origin.branch; }
	if (!origin.commit.empty()) { (*mapping)[KEY_COMMIT] = origin.commit; }
}

auto map_origin_into(node_t::mapping_type* mapping, const origin_url& origin) -> void {
	(*mapping)[KEY_URL] = origin.url;
	if (!origin.md5.empty()) {
		(*mapping)[KEY_MD5] = origin.md5;
	}
}

auto map_into(node_t::mapping_type* mapping, const dip::origin& origin) -> void {
	std::visit([mapping](const auto& origin) { map_origin_into(mapping, origin); }, origin);
}

auto map_package_names_into(node_t::mapping_type* mapping, const std::pmr::vector<std::pmr::string>& package_names) -> void {
	if (!package_names.empty()) {
		auto pkg_names_node = node_t::sequence_type{};
		for (const auto& pkg_name : package_names) {
			pkg_names_node.push_back(pkg_name);
		}
		(*mapping)[KEY_PACKAGE_NAMES] = std::move(pkg_names_node);
	}
}

[[nodiscard]]
auto to_yaml(context* ctx, const dip::dep& dep) -> node_t::mapping_type {
	auto mapping = node_t::mapping_type{};
	mapping[KEY_NAME] = dep.name;
	map_package_names_into(&mapping, dep.package_names);
	map_into(&mapping, dep.origin);
	if (!dep.cmake_options.any.empty()) { mapping[KEY_CMAKE_OPTIONS]     = join<std::pmr::string>(ctx, dep.cmake_options.any, " "); }
	if (!dep.cmake_options.mac.empty()) { mapping[KEY_CMAKE_OPTIONS_MAC] = join<std::pmr::string>(ctx, dep.cmake_options.mac, " "); }
	if (!dep.cmake_options.lin.empty()) { mapping[KEY_CMAKE_OPTIONS_LIN] = join<std::pmr::string>(ctx, dep.cmake_options.lin, " "); }
	if (!dep.cmake_options.win.empty()) { mapping[KEY_CMAKE_OPTIONS_WIN] = join<std::pmr::string>(ctx, dep.cmake_options.win, " "); }
	return mapping;
}

auto save_to(context* ctx, const yml_registry& registry, const std::filesystem::path& path) -> void {
	auto tmp_path = path;
	tmp_path.replace_extension(".tmp");
	auto root = node_t::sequence_type{};
	for (const auto& dep : registry.deps) {
		root.push_back(to_yaml(ctx, dep));
	}
	auto string = node_t::serialize(root);
	write_text_to_file(tmp_path, string);
	std::filesystem::rename(tmp_path, path);
	std::filesystem::remove(tmp_path);
	ctx->log->detail(pmr_format(ctx, "Saved registry to '{}'", path.string()));
}

auto save_to(context* ctx, const yml_meta& meta, const std::filesystem::path& path) -> void {
	auto tmp_path = path;
	tmp_path.replace_extension(".tmp");
	auto root = node_t::mapping_type{};
	root[KEY_VERSION] = meta.version;
	auto string = node_t::serialize(root);
	write_text_to_file(tmp_path, string);
	std::filesystem::rename(tmp_path, path);
	std::filesystem::remove(tmp_path);
	ctx->log->detail(pmr_format(ctx, "Saved meta info to '{}'", path.string()));
}

} // dip
