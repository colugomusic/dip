#include "args.hpp"
#include "cmake.hpp"
#include "cmake-options.hpp"
#include "colors.hpp"
#include "list-util.hpp"
#include "git.hpp"
#include "md5.hpp"
#include "requirements.hpp"
#include "version.hpp"
#include "wget.hpp"
#include "yaml.hpp"
#include "zip.hpp"
#include <string>
#include <vector>

namespace dip {

using std::string_view_literals::operator""sv;

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

using ancestry = std::pmr::vector<std::pmr::string>;

struct work_requested {
	std::pmr::vector<std::pmr::string> cfg;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
	std::pmr::vector<std::pmr::string> reinstall;
};

struct collected_dep {
	dip::dep dep;
	dip::ancestry ancestry;
	std::pmr::string version;
	std::pmr::vector<std::pmr::string> cmake_options;
	int depth = 0;
};

using collected_deps = std::pmr::vector<collected_dep>;

struct collector_work_to_do {
	std::pmr::vector<std::pmr::string> deps;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
};

struct installer_work_to_do {
	std::pmr::vector<std::pmr::string> cmake_configs;
	std::pmr::vector<std::pmr::string> reinstall;
};

struct collector_result {
	std::pmr::vector<std::pmr::string> just_acquired_deps;
	dip::collected_deps collected_deps;
};

struct collector {
	dip::ancestry ancestry;
	yml_registry registry;
	collector_work_to_do work_to_do;
	collector_result* result = nullptr;
};

struct installer {
	installer_work_to_do work_to_do;
	std::pmr::vector<std::pmr::string> at_least_one_dep_was_installed_for_this_parent;
};

struct state {
	dip::dirs dirs;
	dip::prog_paths prog_paths;
	yml_project_settings project_settings;
	dip::work_requested work_requested;
};

auto print(const context* ctx, log_debug v)        { std::cout << pmr_format(ctx, "DEBUG :::::::: {}\n", v.v); }
auto print(const context* ctx, log_dep_task v)     { if (ctx->print_options.dep_tasks) { std::cout << pmr_format(ctx, "{}{}{} {}\n", colors::dep, v.dep, colors::reset, v.task); } }
auto print(const context* ctx, log_dep_cfg_task v) { if (ctx->print_options.dep_tasks) { std::cout << pmr_format(ctx, "{}{}{} {}{}{} {}\n", colors::dep, v.dep, colors::reset, colors::cfg, v.cfg, colors::reset, v.task); } }
auto print(const context* ctx, log_detail v)       { if (ctx->print_options.detail)    { std::cout << pmr_format(ctx, "{}{}{}\n", colors::detail, v.v, colors::reset); } }
auto print(const context* ctx, log_error v)        { if (ctx->print_options.errors)    { std::cout << pmr_format(ctx, "\n{}{}{}\n", colors::error, v.v, colors::reset); } }
auto print(const context* ctx, log_info v)         { if (ctx->print_options.info)      { std::cout << pmr_format(ctx, "{}{}{}\n", colors::info, v.v, colors::reset); } }
auto print(const context* ctx, log_warn v)         { if (ctx->print_options.warnings)  { std::cout << pmr_format(ctx, "{}{}{}\n", colors::warning, v.v, colors::reset); } }

[[nodiscard]] auto fn_print_debug(const context* ctx)        { return [ctx](log_debug v)        { print(ctx, v); }; }
[[nodiscard]] auto fn_print_dep_task(const context* ctx)     { return [ctx](log_dep_task v)     { print(ctx, v); }; }
[[nodiscard]] auto fn_print_dep_cfg_task(const context* ctx) { return [ctx](log_dep_cfg_task v) { print(ctx, v); }; }
[[nodiscard]] auto fn_print_detail(const context* ctx)       { return [ctx](log_detail v)       { print(ctx, v); }; }
[[nodiscard]] auto fn_print_error(const context* ctx)        { return [ctx](log_error v)        { print(ctx, v); }; }
[[nodiscard]] auto fn_print_info(const context* ctx)         { return [ctx](log_info v)         { print(ctx, v); }; }
[[nodiscard]] auto fn_print_warning(const context* ctx)      { return [ctx](log_warn v)         { print(ctx, v); }; }

auto print_and_clear_log(const context* ctx) -> void {
	auto fns = logger_fns{
		.debug        = fn_print_debug(ctx),
		.dep_task     = fn_print_dep_task(ctx),
		.dep_cfg_task = fn_print_dep_cfg_task(ctx),
		.detail       = fn_print_detail(ctx),
		.error        = fn_print_error(ctx),
		.info         = fn_print_info(ctx),
		.warn         = fn_print_warning(ctx)
	};
	ctx->log->visit(fns);
	ctx->log->clear();
}

[[nodiscard]]
auto exit_failure(context* ctx) -> int {
    print_and_clear_log(ctx);
	return EXIT_FAILURE;
}

[[nodiscard]]
auto exit_failure(context* ctx, std::string_view what) -> int {
    print_and_clear_log(ctx);
	print(ctx, log_error{to_pmr_string(ctx, what)});
	return EXIT_FAILURE;
}

[[nodiscard]]
auto exit_success(context* ctx) -> int {
    print_and_clear_log(ctx);
	return EXIT_SUCCESS;
}

[[nodiscard]]
auto find_dip_dir(context* ctx, const std::filesystem::path& project_dir) -> std::optional<std::filesystem::path> {
	const auto dip_dir = project_dir / DIR_PROJECT_DIP;
	if (std::filesystem::exists(dip_dir)) {
		ctx->log->detail(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DIP, dip_dir.string()));
		return dip_dir;
	}
	return std::nullopt;
}

[[nodiscard]]
auto get_dep_names(context* ctx, const yml_registry& registry) -> std::pmr::vector<std::pmr::string> {
	auto deps = std::pmr::vector<std::pmr::string>{ctx->mem};
	std::ranges::transform(registry.deps, std::back_inserter(deps), &dep::name);
	return deps;
}

[[nodiscard]]
auto expand_track(context* ctx, const yml_registry& registry, std::pmr::vector<std::pmr::string> list) -> std::pmr::vector<std::pmr::string> {
	if (list.size() == 1 && list.front() == ARG_TRACK_ALL_VALUE) {
		return get_dep_names(ctx, registry);
	}
	return list;
}

[[nodiscard]]
auto get_cmake_configs_to_process(context*, const yml_project_settings& settings, const dip::work_requested& work_requested) -> std::pmr::vector<std::pmr::string> {
	if (!work_requested.cfg.empty()) { return work_requested.cfg; }
	else                             { return settings.cmake_configs; }
}

auto print_info_about_default_directories(context* ctx, const dip::args& args, const std::filesystem::path& sys_cache_dir, const std::filesystem::path& cache, const std::filesystem::path& root) -> void {
	const auto arg_cache = args.cache.v;
	const auto arg_root  = args.root.v;
	if (arg_cache && arg_root) {
		return;
	}
	ctx->log->detail(pmr_format(ctx, "System cache folder is: '{}'", sys_cache_dir.string()));
	if (!arg_cache && !arg_root) {
		ctx->log->info(pmr_format(ctx,
			"Using these default directory paths because you didn't specify them:\n"
			" - cache: '{}' (override with --cache path/to/cache)\n"
			" - root:  '{}' (override with --root path/to/root)",
			cache.string(), root.string()
		));
		if (os::get_platform() == os::platform::win) {
			ctx->log->info("Note that on Windows if the cache path is too long then it could cause problems during building of dependencies.");
		}
		return;
	}
	if (!arg_cache) {
		ctx->log->info(pmr_format(ctx,
			"Using '{}' as cache because you didn't specify one.\n"
			"If you're not happy with this then specify a cache with --cache \"path/to/cache\".",
			cache.string()));
		if (os::get_platform() == os::platform::win) {
			ctx->log->info("Note that on Windows if this path is too long then it could cause problems during building of dependencies.");
		}
		return;
	}
	if (!arg_root) {
		ctx->log->info(pmr_format(ctx,
			"Using '{}' as root because you didn't specify one.\n"
			"If you're not happy with this then specify a root with --root \"path/to/root\"",
			root.string()));
		return;
	}
}

auto get_dirs(context* ctx, const dip::args& args, std::string_view project_name) -> dip::dirs {
	const auto sys_cache_dir = os::get_system_cache_dir();
	auto cache = args.cache.v.value_or(sys_cache_dir / "dip-cache");
	auto root  = args.root.v.value_or(sys_cache_dir / "dip-root" / project_name);
	print_info_about_default_directories(ctx, args, sys_cache_dir, cache, root);
	return dip::dirs{
		.cache   = cache,
		.root    = root,
	};
}

auto get_work_requested(const dip::args& args) -> dip::work_requested {
	return dip::work_requested{
		.cfg          = args.cfg.v,
		.track        = args.track.v,
		.reacquire    = args.reacquire.v,
		.reinstall    = args.reinstall.v,
	};
}

auto get_prog_paths(const requirements& reqs) -> dip::prog_paths {
	return dip::prog_paths{
		.cmake = reqs.cmake_path,
		.git   = reqs.git_path,
		.wget  = reqs.wget_path,
		.zip   = reqs.zip_path
	};
}

[[nodiscard]]
auto make_self_dep(context* ctx, std::string_view project_name, const std::filesystem::path& project_dir) -> dep {
	if (project_name.empty()) {
		throw std::runtime_error{"Can't self-install this project because settings.yml doesn't contain a 'name' key."};
	}
	return dip::dep{
		.name   = to_pmr_string(ctx, project_name),
		.origin = project_dir
	};
}

[[nodiscard]]
auto make_ancestry(context* ctx, dip::ancestry parent_ancestry, std::string_view dep_name) -> dip::ancestry {
	auto list = std::move(parent_ancestry);
	list.push_back(to_string(ctx, dep_name));
	return list;
}

[[nodiscard]]
auto sort_deps_into_processing_order(std::pmr::vector<std::pmr::string> list, const yml_registry& registry) -> std::pmr::vector<std::pmr::string> {
	const auto fn_less = [&registry](const std::pmr::string& a, const std::pmr::string& b) {
		return get_position_in_registry(registry, a) < get_position_in_registry(registry, b);
	};
	std::ranges::sort(list, fn_less);
	return list;
}

[[nodiscard]]
auto get_collector_work_to_do(context* ctx, const yml_registry& registry, const dip::work_requested& work_requested) -> collector_work_to_do {
	auto track     = expand_track(ctx, registry, work_requested.track);
	auto reacquire = work_requested.reacquire;
	auto reinstall = work_requested.reinstall;
	return collector_work_to_do{
		.deps          = sort_deps_into_processing_order(get_dep_names(ctx, registry), registry),
		.track         = sort_and_remove_duplicates(ctx, track),
		.reacquire     = sort_and_remove_duplicates(ctx, reacquire),
	};
}

[[nodiscard]]
auto get_installer_work_to_do(context* ctx, const yml_project_settings& settings, const dip::work_requested& work_requested) -> installer_work_to_do {
	auto reinstall = work_requested.reinstall;
	return installer_work_to_do{
		.cmake_configs = get_cmake_configs_to_process(ctx, settings, work_requested),
		.reinstall     = sort_and_remove_duplicates(ctx, reinstall),
	};
}

[[nodiscard]]
auto init_collector(context* ctx, dip::ancestry ancestry, yml_registry registry, dip::work_requested work_requested, dip::collector_result* result) -> collector {
	auto work_to_do = get_collector_work_to_do(ctx, registry, work_requested);
	return dip::collector{
		.ancestry   = std::move(ancestry),
		.registry   = std::move(registry),
		.work_to_do = std::move(work_to_do),
		.result     = result
	};
}

[[nodiscard]]
auto init_installer(context* ctx, const yml_project_settings& settings, const dip::work_requested& work_requested) -> installer {
	return installer{
		.work_to_do = get_installer_work_to_do(ctx, settings, work_requested)
	};
}

[[nodiscard]]
auto init_state(context* ctx, const dip::args& args, const requirements& reqs, const std::filesystem::path& dip_dir) -> state {
	const auto project_settings = read_project_settings_yml(ctx, dip_dir);
	return dip::state{
		.dirs             = get_dirs(ctx, args, project_settings.name),
		.prog_paths       = get_prog_paths(reqs),
		.project_settings = project_settings,
		.work_requested   = get_work_requested(args)
	};
}

[[nodiscard]]
auto init_collector_for_dependency_subprocessing(context* ctx, dip::ancestry parent_ancestry, dip::work_requested work_requested, std::string_view dep_name, const std::filesystem::path& registry_path, collector_result* result) -> dip::collector {
	auto ancestry = make_ancestry(ctx, parent_ancestry, dep_name);
	auto registry = read_registry_yml(ctx, registry_path);
	return init_collector(ctx, std::move(ancestry), std::move(registry), std::move(work_requested), result);
}

[[nodiscard]]
auto make_print_options(const dip::args& args) -> print_options {
	return print_options{
		.dep_tasks = !(args.quiet.v || args.stfu.v),
		.detail    = args.verbose.v,
		.errors    = !args.stfu.v,
		.warnings  = !(args.quiet.v || args.stfu.v),
		.info      = !(args.quiet.v || args.stfu.v)
	};
}

[[nodiscard]]
auto has_empty_track_commit(const dip::dep& dep) -> bool {
	if (const auto origin = std::get_if<origin_git_tracked_branch>(&dep.origin)) {
		return origin->commit.empty();
	}
	return false;
}

[[nodiscard]]
auto user_requested_reacquire(const dip::collector_work_to_do& work_to_do, std::string_view name) -> bool {
	return std::ranges::binary_search(work_to_do.reacquire, name);
}

[[nodiscard]]
auto user_requested_reinstall(const dip::installer_work_to_do& work_to_do, std::string_view name) -> bool {
	return std::ranges::binary_search(work_to_do.reinstall, name);
}

[[nodiscard]]
auto user_requested_track(context*, const dip::collector_work_to_do& work_to_do, std::string_view name) -> bool {
	return std::ranges::binary_search(work_to_do.track, name);
}

[[nodiscard]]
auto have_source_code(context* ctx, const dip::state& state, std::string_view dep_name, std::string_view version) -> bool {
	const auto src_dir_path    = make_src_dir_path(ctx, state.dirs, version);
	const auto cmakelists_path = find_file_in_dir(ctx, src_dir_path, "CMakeLists.txt");
	if (cmakelists_path) {
		ctx->log->detail(pmr_format(ctx, "Found CMakeLists.txt for '{}' at '{}'", dep_name, cmakelists_path->string()));
	}
	else {
		ctx->log->detail(pmr_format(ctx, "No CMakeLists.txt found for '{}' in '{}'", dep_name, src_dir_path.string()));
	}
	return cmakelists_path.has_value();
}

auto remember_that_a_subdependency_of_this_was_installed(context* ctx, dip::installer* installer, std::string_view parent_name) -> void {
	auto list = &installer->at_least_one_dep_was_installed_for_this_parent;
	if (const auto pos = std::ranges::find(*list, parent_name); pos == list->end()) {
		list->push_back(to_pmr_string(ctx, parent_name));
	}
}

[[nodiscard]]
auto were_any_subdependencies_of_this_installed(const dip::installer* installer, std::string_view parent_name) -> bool {
	const auto& list = installer->at_least_one_dep_was_installed_for_this_parent;
	return std::ranges::find(list, parent_name) != list.end();
}

[[nodiscard]]
auto was_this_just_acquired(const dip::collector_result& result, std::string_view name) {
	return std::ranges::find(result.just_acquired_deps, name) != result.just_acquired_deps.end();
}

[[nodiscard]]
auto to_track(context* ctx, const dip::collector& collector, const dip::dep& dep) -> bool {
	const auto empty_track_commit = has_empty_track_commit(dep);
	const auto user_requested     = user_requested_track(ctx, collector.work_to_do, dep.name);
	const auto origin_is_track    = std::holds_alternative<origin_git_tracked_branch>(dep.origin);
	return empty_track_commit || (user_requested && origin_is_track);
}

[[nodiscard]]
auto to_acquire(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, std::string_view version) -> bool {
	return
		!have_source_code(ctx, state, dep_name, version) ||
		user_requested_reacquire(collector.work_to_do, dep_name);
}

[[nodiscard]]
auto to_install(context* ctx, const dip::state& state, const dip::collector_result& collector_result, const dip::installer& installer, std::string_view dep_name, std::string_view cmake_config) -> bool {
	return
		!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, dep_name, cmake_config) ||
		user_requested_reinstall(installer.work_to_do, dep_name) ||
		were_any_subdependencies_of_this_installed(&installer, dep_name) ||
		was_this_just_acquired(collector_result, dep_name);
}

[[nodiscard]]
auto make_ancestry_string(context* ctx, const std::pmr::vector<std::pmr::string>& ancestry) -> std::pmr::string {
	if (ancestry.empty()) { return std::pmr::string{ctx->mem}; }
	return join<std::pmr::string>(ctx, ancestry, " -> ");
}

[[nodiscard]]
auto decorate(context* ctx, const dip::ancestry& ancestry, std::string_view dep_name) -> std::pmr::string {
	if (ancestry.empty()) { return to_pmr_string(ctx, dep_name); }
	else                  { return pmr_format(ctx, "{} -> {}", make_ancestry_string(ctx, ancestry), dep_name); }
}

[[nodiscard]]
auto decorate(context* ctx, const dip::collected_dep& cdep) -> std::pmr::string {
	return decorate(ctx, cdep.ancestry, cdep.dep.name);
}

[[nodiscard]]
auto update_track_commit(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, origin_git_tracked_branch git) -> bool {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Fetching latest commit from '{}'", git.url));
	print_and_clear_log(ctx);
	const auto new_hash = get_latest_git_commit_hash(ctx, state.prog_paths, git.url, git.branch);
	ctx->log->detail(pmr_format(ctx, "latest commit is '{}'", new_hash));
	if (new_hash != git.commit) {
		git.commit = new_hash;
		dep->origin = git;
		ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Updated commit to '{}'", git.commit));
		return true;
	}
	ctx->log->detail("commit is already at latest");
	return false;
}

auto update_track_commit(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector) -> bool {
	assert (std::holds_alternative<origin_git_tracked_branch>(dep->origin));
	return update_track_commit(ctx, dep, state, collector, std::get<origin_git_tracked_branch>(dep->origin));
}

auto extract_to(context* ctx, const dip::state& state, const dip::collector& collector, const dip::dep& dep, const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir_path) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep.name), pmr_format(ctx, "Extracting '{}'", archive_path.filename().string()));
	print_and_clear_log(ctx);
	extract_to(ctx, state.prog_paths, archive_path, dest_dir_path);
}

auto md5_check_or_update(context* ctx, dip::dep* dep, const std::filesystem::path& file, origin_url origin) -> void {
	const auto md5 = calc_md5(ctx, file);
	if (origin.md5.empty()) {
		origin.md5 = md5;
		dep->origin = origin;
	}
	else {
		if (md5 != origin.md5) {
			throw std::runtime_error(std::format("MD5 mismatch for downloaded file '{}'. Expected '{}', got '{}'", file.string(), origin.md5, md5));
		}
	}
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, std::string_view version, const std::filesystem::path& origin) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Copying source code from '{}'", origin.string()));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	const auto copy_options =
		std::filesystem::copy_options::recursive |
		std::filesystem::copy_options::overwrite_existing;
	std::filesystem::copy(origin, src_dir_path, copy_options);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, std::string_view version, const origin_git_repo& origin) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, std::string_view version, const origin_git_tracked_branch& origin) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, std::string_view version, const origin_url& origin) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep->name), pmr_format(ctx, "Downloading source code from '{}'", origin.url));
	print_and_clear_log(ctx);
	const auto dl_dir_path     = make_dl_dir_path(ctx, state.dirs, version);
	const auto src_dir_path    = make_src_dir_path(ctx, state.dirs, version);
	const auto downloaded_file = download_file(ctx, state.prog_paths, origin.url, dl_dir_path);
	md5_check_or_update(ctx, dep, downloaded_file, origin);
	extract_to(ctx, state, collector, *dep, downloaded_file, src_dir_path);
}

auto acquire(context* ctx, dip::dep* dep, const dip::state& state, const dip::collector& collector, std::string_view version) -> void {
	std::visit([ctx, dep, &state, &collector, version](const auto& origin) { acquire_src_from_origin(ctx, dep, state, collector, version, origin); }, dep->origin);
}

auto configure(context* ctx, const dip::state& state, const dip::collected_dep& cdep, const std::filesystem::path& src_dir_path, const std::filesystem::path& bld_dir_path, std::span<const std::pmr::string> cmake_options_list, std::string_view cmake_config) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, cdep), to_pmr_string(ctx, cmake_config), pmr_format(ctx, "Configure"));
	print_and_clear_log(ctx);
	const auto install_prefix_path = make_install_prefix_path(state.dirs, cmake_config);
	const auto cmakelists          = find_file_in_dir(ctx, src_dir_path, "CMakeLists.txt");
	if (!cmakelists) {
		throw std::runtime_error(std::format("No CMakeLists.txt found in source directory '{}'", src_dir_path.string()));
	}
	const auto cmake_options_string = get_cmake_options_string(ctx, cmake_options_list);
	cmake_configure(ctx, state.prog_paths, install_prefix_path, cmakelists->parent_path(), bld_dir_path, cmake_config, cmake_options_string);
}

auto build(context* ctx, const dip::state& state, const dip::collected_dep& cdep, const std::filesystem::path& bld_dir_path, std::string_view cmake_config) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, cdep), to_pmr_string(ctx, cmake_config), pmr_format(ctx, "Build"));
	print_and_clear_log(ctx);
	cmake_build(ctx, state.prog_paths, bld_dir_path, cmake_config);
}

auto install(context* ctx, const dip::state& state, const dip::collected_dep& cdep, const std::filesystem::path& bld_dir_path, std::string_view cmake_config) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, cdep), to_pmr_string(ctx, cmake_config), pmr_format(ctx, "Install"));
	print_and_clear_log(ctx);
	cmake_install(ctx, state.prog_paths, bld_dir_path, cmake_config);
	if (!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, cdep.dep.name, cmake_config)) {
		throw std::runtime_error{std::format("CMake could still not find package '{}' after installing it.", cdep.dep.name)};
	}
}

auto configure_build_install(context* ctx, const dip::state& state, const dip::collected_dep& cdep, std::string_view cmake_config) -> void {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, cdep.version);
	const auto bld_dir_path = make_bld_dir_path(ctx, state.dirs, cdep.version, cmake_config);
	configure(ctx, state, cdep, src_dir_path, bld_dir_path, cdep.cmake_options, cmake_config);
	build(ctx, state, cdep, bld_dir_path, cmake_config);
	install(ctx, state, cdep, bld_dir_path, cmake_config);
}

auto run_collector(context* ctx, const dip::state& state, dip::collector* collector, int depth) -> void;
auto run_installer(context* ctx, const dip::state& state, dip::installer* installer) -> void;

[[nodiscard]]
auto get_dep_registry_to_use(context* ctx, const std::filesystem::path& dip_dir, const std::filesystem::path& registry_override) -> std::filesystem::path {
	if (std::filesystem::exists(registry_override)) {
		ctx->log->detail(pmr_format(ctx, "Using registry override file at '{}'", registry_override.string()));
		return registry_override;
	}
	else {
		ctx->log->detail(pmr_format(ctx, "Using registry file at '{}'", (dip_dir / FILENAME_REGISTRY_YML).string()));
		return dip_dir / FILENAME_REGISTRY_YML;
	}
}

[[nodiscard]]
auto run_dip_on(context* ctx, const dip::state& state, const dip::collector& collector, const dip::dep& dep, std::string_view version, const std::pmr::vector<std::pmr::string>& cmake_options, const std::filesystem::path& registry_override, int depth) -> bool {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	if (const auto dip_dir = find_dip_dir(ctx, src_dir_path)) {
		ctx->log->detail(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DIP, dip_dir->string()));
		const auto registry_path = get_dep_registry_to_use(ctx, *dip_dir, registry_override);
		auto dep_collector       = init_collector_for_dependency_subprocessing(ctx, collector.ancestry, state.work_requested, dep.name, registry_path, collector.result);
		run_collector(ctx, state, &dep_collector, depth + 1);
		// Collect self
		auto self_cdep = collected_dep {
			.dep           = dep,
			.ancestry      = collector.ancestry,
			.version       = to_pmr_string(ctx, version),
			.cmake_options = cmake_options,
			.depth         = depth
		};
		collector.result->collected_deps.push_back(std::move(self_cdep));
		save_to(ctx, dep_collector.registry, registry_override);
		return true;
	}
	return false;
}

[[nodiscard]]
auto find_collected_dep(const dip::collector& collector, std::string_view name) -> collected_deps::iterator {
	const auto fn_name_is = [name](const dip::collected_dep& cdep) { return cdep.dep.name == name; };
	return std::ranges::find_if(collector.result->collected_deps, fn_name_is);
}

[[nodiscard]]
auto get_parent(context* ctx, const dip::ancestry& ancestry) -> std::pmr::string {
	if (ancestry.empty()) { return std::pmr::string{ctx->mem}; }
	else                  { return ancestry.back(); }
}

auto move_before(dip::collected_deps* list, std::string_view move_this, std::string_view before_this) -> void {
	const auto fn_is_to_move = [move_this](const dip::collected_dep& cdep) { return cdep.dep.name == move_this; };
	const auto fn_is_before  = [before_this](const dip::collected_dep& cdep) { return cdep.dep.name == before_this; };
	if (const auto pos_to_move = std::ranges::find_if(*list, fn_is_to_move); pos_to_move != list->end()) {
		if (const auto pos_before = std::ranges::find_if(*list, fn_is_before); pos_before != list->end()) {
			const auto move_cdep = *pos_to_move;
			list->erase(pos_to_move);
			list->insert(pos_before, std::move(move_cdep));
		}
	}
}

auto run_collector(context* ctx, const dip::state& state, dip::collector* collector, std::string_view name, int depth) -> void {
	auto existing_cdep = find_collected_dep(*collector, name);
	if (existing_cdep != collector->result->collected_deps.end()) {
		if (depth >= existing_cdep->depth) {
			// If we already collected a dep with this name and its depth
			// is less than our current depth, keep the existing dep but
			// just move it so that it's processed before the parent of
			// this one.
			if (const auto parent_name = get_parent(ctx, collector->ancestry); !parent_name.empty()) {
				move_before(&collector->result->collected_deps, name, parent_name);
			}
			return;
		}
	}
	auto dep = get_dep(&collector->registry, name);
	if (to_track(ctx, *collector, *dep)) {
		update_track_commit(ctx, dep, state, *collector);
	}
	const auto cmake_options     = get_cmake_options_list(ctx, os::get_platform(), state.project_settings.cmake_options, dep->cmake_options);
	const auto version           = make_version_string(ctx, dep->origin, cmake_options);
	const auto registry_override = make_registry_override_path(state.dirs, version);
	ctx->log->detail(pmr_format(ctx, "version: '{}'", version));
	if (to_acquire(ctx, state, *collector, dep->name, version)) {
		acquire(ctx, dep, state, *collector, version);
		collector->result->just_acquired_deps.push_back(to_pmr_string(ctx, name));
		remove_if_exists(registry_override);
	}
	if (run_dip_on(ctx, state, *collector, *dep, version, cmake_options, registry_override, depth)) {
		// If that returned true then the dependency is also using dip to handle its
		// own dependencies.
		return;
	}
	if (existing_cdep != collector->result->collected_deps.end()) {
		existing_cdep->ancestry      = collector->ancestry;
		existing_cdep->version       = version;
		existing_cdep->cmake_options = cmake_options;
		existing_cdep->depth         = depth;
		return;
	}
	auto cdep = collected_dep {
		.dep           = *dep,
		.ancestry      = collector->ancestry,
		.version       = version,
		.cmake_options = cmake_options,
		.depth         = depth
	};
	collector->result->collected_deps.push_back(std::move(cdep));
}

auto install(context* ctx, const dip::state& state, const dip::collector_result& collector_result, dip::installer* installer, const collected_dep& cdep) -> void {
	for (const auto cmake_config : installer->work_to_do.cmake_configs) {
		if (to_install(ctx, state, collector_result, *installer, cdep.dep.name, cmake_config)) {
			configure_build_install(ctx, state, cdep, cmake_config);
			if (const auto parent = get_parent(ctx, cdep.ancestry); !parent.empty()) {
				remember_that_a_subdependency_of_this_was_installed(ctx, installer, parent);
			}
		}
	}
	ctx->log->dep_task(decorate(ctx, cdep), "Ready");
}

auto run_collector(context* ctx, const dip::state& state, dip::collector* collector, int depth) -> void {
	for (const auto& name : collector->work_to_do.deps) {
		print_and_clear_log(ctx);
		run_collector(ctx, state, collector, name, depth);
	}
}

auto run_installer(context* ctx, const dip::state& state, const dip::collector_result& collector_result, dip::installer* installer) -> void {
	for (const auto& cdep : collector_result.collected_deps) {
		print_and_clear_log(ctx);
		install(ctx, state, collector_result, installer, cdep);
	}
}

auto print_cmake_prefix_help(context* ctx, const dip::dirs& dirs, std::span<const std::pmr::string> cmake_configs) -> void {
	ctx->log->info("\nHere are your CMake prefix paths:\n");
	for (const auto& cmake_config : cmake_configs) {
		ctx->log->info(pmr_format(ctx, "  For a {} build:\n    -DCMAKE_PREFIX_PATH=\"{}\"\n", cmake_config, make_install_prefix_path(dirs, cmake_config).string()));
	}
}

[[nodiscard]]
auto init_collector_result(context* ctx) -> dip::collector_result {
	return {
		.just_acquired_deps = std::pmr::vector<std::pmr::string>{ctx->mem},
		.collected_deps     = dip::collected_deps{ctx->mem}
	};
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (const auto reqs = check_requirements(ctx)) {
		if (const auto dip_dir = find_dip_dir(ctx, args.project_dir.v)) {
			const auto no_ancestry      = dip::ancestry{ctx->mem};
			const auto initial_registry = read_registry_yml(ctx, *dip_dir / FILENAME_REGISTRY_YML);
			auto state = init_state(ctx, args, *reqs, *dip_dir);
			auto collector_result = init_collector_result(ctx);
			auto collector = init_collector(ctx, no_ancestry, initial_registry, state.work_requested, &collector_result);
			run_collector(ctx, state, &collector, 0);
			save_to(ctx, collector.registry, *dip_dir / FILENAME_REGISTRY_YML);
			auto installer = init_installer(ctx, state.project_settings, state.work_requested);
			run_installer(ctx, state, collector_result, &installer);
			print_cmake_prefix_help(ctx, state.dirs, installer.work_to_do.cmake_configs);
			return exit_success(ctx);
		}
		ctx->log->error(pmr_format(ctx, "No '{}' directory found in project directory '{}'", DIR_PROJECT_DIP, args.project_dir.v.string()));
	}
	return exit_failure(ctx);
}

[[nodiscard]]
auto make_context(logger* log, mem_res* mem) -> context {
	return context{
		.mem       = mem,
		.log       = log,
		.cwd       = std::filesystem::current_path(),
		.env_paths = os::get_env_paths(mem)
	};
}

[[nodiscard]]
auto main(int argc, const char* argv[]) -> int {
	auto mem = std::pmr::monotonic_buffer_resource{16_MB};
	auto log = logger{};
	auto ctx = make_context(&log, &mem);
	try                               { return happy_path(&ctx, argc, argv); }
	catch (const std::exception& err) { return exit_failure(&ctx, err.what()); }
	catch (...)                       { return exit_failure(&ctx, "Unknown exception."); }
}

} // dip

auto main(int argc, const char* argv[]) -> int {
	return dip::main(argc, argv);
}
