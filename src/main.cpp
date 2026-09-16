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
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace dip {

using std::string_view_literals::operator""sv;

auto operator""_MB(unsigned long long v) -> uint64_t { return 1024 * 1024 * v; }

using ancestry = std::pmr::vector<std::pmr::string>;

struct work_requested {
	std::pmr::vector<std::pmr::string> cfg;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
	std::pmr::vector<std::pmr::string> reinstall;
	bool full_package_check = false;
};

struct collected_dep {
	dip::dep dep;
	dip::ancestry ancestry;
	std::pmr::string version;
	std::pmr::vector<std::pmr::string> cmake_options;
	std::pmr::vector<std::pmr::string> package_names;
	std::pmr::vector<std::pmr::string> dependencies;
};

using collected_deps = std::pmr::vector<collected_dep>;

struct commit_update {
	std::pmr::string name;
	std::pmr::string new_commit;
};

struct md5_update {
	std::pmr::string name;
	std::pmr::string new_md5;
};

struct acquire_result {
	std::optional<md5_update> md5_update;
};

struct registry_update {
	std::pmr::vector<commit_update> commit_updates;
	std::pmr::vector<md5_update> md5_updates;
};

struct collector_work_to_do {
	std::pmr::vector<std::pmr::string> deps;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
};

struct installer_work_to_do {
	std::pmr::vector<std::pmr::string> cmake_configs;
	std::pmr::vector<std::pmr::string> reinstall;
};

struct subcollector_result {
	std::pmr::vector<std::pmr::string> just_acquired_deps;
	std::pmr::vector<std::pmr::string> package_names;
	std::pmr::vector<std::pmr::string> subdeps;
	dip::collected_deps collected_deps;
};

struct collector_result {
	dip::registry_update registry_update;
	std::pmr::vector<std::pmr::string> just_acquired_deps;
	dip::collected_deps collected_deps;
};

struct collector {
	dip::ancestry ancestry;
	collector_work_to_do work_to_do;
};

struct installer {
	installer_work_to_do work_to_do;
	// List of deps that were just installed by this installer.
	std::pmr::vector<std::pmr::string> install_log;
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
	if (arg_cache && !arg_root.empty()) {
		return;
	}
	ctx->log->detail(pmr_format(ctx, "System cache folder is: '{}'", sys_cache_dir.string()));
	if (!arg_cache && arg_root.empty()) {
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
	if (arg_root.empty()) {
		ctx->log->info(pmr_format(ctx,
			"Using '{}' as root because you didn't specify one.\n"
			"If you're not happy with this then specify a root with --root \"path/to/root\"",
			root.string()));
		return;
	}
}

[[nodiscard]]
auto get_cache_dir(const std::filesystem::path& sys_cache_dir, const dip::args& args) -> std::filesystem::path {
	return args.cache.v.value_or(sys_cache_dir / "dip-cache");
}

[[nodiscard]]
auto get_root_dir(const std::filesystem::path& sys_cache_dir, const dip::args& args, std::string_view project_name) -> std::filesystem::path {
	if (args.root.v.empty()) { return sys_cache_dir / "dip-root" / project_name; }
	else                     { return args.root.v.front(); }
}

[[nodiscard]]
auto get_root_dirs(context* ctx, const std::filesystem::path& sys_cache_dir, const dip::args& args) -> std::pmr::vector<std::filesystem::path> {
	if (args.root.v.empty()) { return {{sys_cache_dir / "dip-root"}, ctx->mem}; }
	else                     { return args.root.v; }
}

auto get_dirs(context* ctx, const dip::args& args, std::string_view project_name) -> dip::dirs {
	const auto sys_cache_dir = os::get_system_cache_dir();
	auto cache = get_cache_dir(sys_cache_dir, args);
	auto root  = get_root_dir(sys_cache_dir, args, project_name);
	print_info_about_default_directories(ctx, args, sys_cache_dir, cache, root);
	return dip::dirs{
		.cache   = cache,
		.root    = root,
	};
}

auto get_work_requested(const dip::args& args) -> dip::work_requested {
	return dip::work_requested{
		.cfg                = args.cfg.v,
		.track              = args.track.v,
		.reacquire          = args.reacquire.v,
		.reinstall          = args.reinstall.v,
		.full_package_check = args.check.v
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
auto make_ancestry(context* ctx, dip::ancestry parent_ancestry, std::string_view dep_name) -> dip::ancestry {
	auto list = std::move(parent_ancestry);
	list.push_back(to_string(ctx, dep_name));
	return list;
}

[[nodiscard]]
auto get_collector_work_to_do(context* ctx, const yml_registry& registry, const dip::work_requested& work_requested) -> collector_work_to_do {
	auto track     = expand_track(ctx, registry, work_requested.track);
	auto reacquire = work_requested.reacquire;
	auto reinstall = work_requested.reinstall;
	return collector_work_to_do{
		.deps          = get_dep_names(ctx, registry),
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
auto init_collector(context* ctx, dip::ancestry ancestry, yml_registry registry, dip::work_requested work_requested) -> collector {
	auto work_to_do = get_collector_work_to_do(ctx, registry, work_requested);
	return dip::collector{
		.ancestry   = std::move(ancestry),
		.work_to_do = std::move(work_to_do),
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

[[nodiscard]]
auto were_any_subdependencies_of_this_installed(const dip::installer& installer, const collected_dep& cdep) -> bool {
	const auto fn_was_just_installed = [&installer](std::string_view name) -> bool { return std::ranges::find(installer.install_log, name) != std::cend(installer.install_log); };
	return std::ranges::any_of(cdep.dependencies, fn_was_just_installed);
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
auto get_package_names_to_search_for(context* ctx, std::string_view dep_name, std::span<const std::pmr::string> package_names) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (package_names.empty()) {
		list.push_back(to_pmr_string(ctx, dep_name));
		return list;
	}
	std::ranges::copy(package_names, std::back_inserter(list));
	return list;
}

[[nodiscard]]
auto make_meta_file_path(const dip::dirs& dirs, const collected_dep& cdep, std::string_view cmake_config) -> std::filesystem::path {
	const auto meta_dir      = make_install_meta_path(dirs, cmake_config);
	const auto meta_filename = cdep.dep.name + ".yml";
	std::filesystem::create_directories(meta_dir);
	return (meta_dir / meta_filename);
}

[[nodiscard]]
auto wrong_version_installed(context* ctx, const dip::dirs& dirs, const collected_dep& cdep, std::string_view cmake_config) -> bool {
	const auto meta_file_path = make_meta_file_path(dirs, cdep, cmake_config);
	const auto meta           = read_meta_yml(ctx, meta_file_path);
	if (meta.version != cdep.version) {
		ctx->log->detail(pmr_format(ctx, "Installed version '{}' does not match required version '{}'", meta.version, cdep.version));
		return true;
	}
	return false;
}

[[nodiscard]]
auto to_install(context* ctx, const dip::state& state, const dip::collector_result& collector_result, const dip::installer& installer, const collected_dep& cdep, std::string_view cmake_config) -> bool {
	if (state.work_requested.full_package_check) {
		if (!cmake_packages_can_be_found(ctx, state.dirs, state.prog_paths, get_package_names_to_search_for(ctx, cdep.dep.name, cdep.package_names), cmake_config)) {
			return true;
		}
	}
	return
		wrong_version_installed(ctx, state.dirs, cdep, cmake_config) ||
		user_requested_reinstall(installer.work_to_do, cdep.dep.name) ||
		were_any_subdependencies_of_this_installed(installer, cdep) ||
		was_this_just_acquired(collector_result, cdep.dep.name);
}

[[nodiscard]]
auto make_ancestry_string(context* ctx, const std::pmr::vector<std::pmr::string>& ancestry) -> std::pmr::string {
	if (ancestry.empty()) { return std::pmr::string{ctx->mem}; }
	return join<std::pmr::string>(ctx, ancestry, " | ");
}

[[nodiscard]]
auto decorate(context* ctx, const dip::ancestry& ancestry, std::string_view dep_name) -> std::pmr::string {
	if (ancestry.empty()) { return to_pmr_string(ctx, dep_name); }
	else                  { return pmr_format(ctx, "{} | {}", make_ancestry_string(ctx, ancestry), dep_name); }
}

[[nodiscard]]
auto decorate(context* ctx, const dip::collected_dep& cdep) -> std::pmr::string {
	return decorate(ctx, cdep.ancestry, cdep.dep.name);
}

[[nodiscard]]
auto update_track_commit(context* ctx, const dip::dep& dep, const dip::state& state, const dip::collector& collector, const origin_git_tracked_branch& git) -> std::optional<commit_update> {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep.name), pmr_format(ctx, "Fetching latest commit from '{}'", git.url));
	print_and_clear_log(ctx);
	const auto new_hash = get_latest_git_commit_hash(ctx, state.prog_paths, git.url, git.branch);
	ctx->log->detail(pmr_format(ctx, "Latest commit is '{}'", new_hash));
	if (new_hash != git.commit) {
		ctx->log->dep_task(decorate(ctx, collector.ancestry, dep.name), pmr_format(ctx, "Updating commit to '{}'", git.commit));
		return commit_update{.name = dep.name, .new_commit = new_hash};
	}
	ctx->log->detail("commit is already at latest");
	return std::nullopt;
}

[[nodiscard]]
auto update_track_commit(context* ctx, const dip::dep& dep, const dip::state& state, const dip::collector& collector) -> std::optional<commit_update> {
	assert (std::holds_alternative<origin_git_tracked_branch>(dep.origin));
	return update_track_commit(ctx, dep, state, collector, std::get<origin_git_tracked_branch>(dep.origin));
}

auto extract_to(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir_path) -> void {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep_name), pmr_format(ctx, "Extracting '{}'", archive_path.filename().string()));
	print_and_clear_log(ctx);
	extract_to(ctx, state.prog_paths, archive_path, dest_dir_path);
}

[[nodiscard]]
auto md5_check_or_update(context* ctx, std::string_view dep_name, const std::filesystem::path& file, origin_url origin) -> std::optional<md5_update> {
	const auto md5 = calc_md5(ctx, file);
	if (origin.md5.empty()) {
		return md5_update{.name = to_string(ctx, dep_name), .new_md5 = md5};
	}
	if (md5 != origin.md5) {
		throw std::runtime_error(std::format("MD5 mismatch for downloaded file '{}'. Expected '{}', got '{}'", file.string(), origin.md5, md5));
	}
	return std::nullopt;
}

[[nodiscard]]
auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, std::string_view version, const std::filesystem::path& origin) -> acquire_result {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep_name), pmr_format(ctx, "Copying source code from '{}'", origin.string()));
	print_and_clear_log(ctx);
	const auto copy_options =
		std::filesystem::copy_options::recursive |
		std::filesystem::copy_options::overwrite_existing;
	std::filesystem::create_directories(src_dir_path);
	std::filesystem::copy(origin, src_dir_path, copy_options);
	return {};
}

[[nodiscard]]
auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, std::string_view version, const origin_git_repo& origin) -> acquire_result {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep_name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
	return {};
}

[[nodiscard]]
auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, std::string_view version, const origin_git_tracked_branch& origin) -> acquire_result {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep_name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
	return {};
}

[[nodiscard]]
auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::collector& collector, std::string_view dep_name, std::string_view version, const origin_url& origin) -> acquire_result {
	ctx->log->dep_task(decorate(ctx, collector.ancestry, dep_name), pmr_format(ctx, "Downloading source code from '{}'", origin.url));
	print_and_clear_log(ctx);
	const auto dl_dir_path     = make_dl_dir_path(ctx, state.dirs, version);
	const auto src_dir_path    = make_src_dir_path(ctx, state.dirs, version);
	const auto downloaded_file = download_file(ctx, state.prog_paths, origin.url, dl_dir_path);
	auto result = acquire_result{};
	if (auto update = md5_check_or_update(ctx, dep_name, downloaded_file, origin)) {
		result.md5_update = std::move(*update);
	}
	extract_to(ctx, state, collector, dep_name, downloaded_file, src_dir_path);
	return result;
}

[[nodiscard]]
auto acquire(context* ctx, const dip::state& state, const dip::collector& collector, const dip::dep& dep, std::string_view version) -> acquire_result {
	return std::visit([ctx, &state, &collector, &dep, version](const auto& origin) { return acquire_src_from_origin(ctx, state, collector, dep.name, version, origin); }, dep.origin);
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

auto write_successful_install_meta_file(context* ctx, const dip::dirs& dirs, const dip::collected_dep& cdep, std::string_view cmake_config) -> void {
	save_to(ctx, yml_meta{.version = cdep.version}, make_meta_file_path(dirs, cdep, cmake_config));
}

auto install(context* ctx, const dip::state& state, const dip::collected_dep& cdep, const std::filesystem::path& bld_dir_path, std::string_view cmake_config) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, cdep), to_pmr_string(ctx, cmake_config), pmr_format(ctx, "Install"));
	print_and_clear_log(ctx);
	cmake_install(ctx, state.prog_paths, bld_dir_path, cmake_config);
	for (const auto& package_name : get_package_names_to_search_for(ctx, cdep.dep.name, cdep.package_names)) {
		if (!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, package_name, cmake_config)) {
			throw std::runtime_error{std::format("CMake could still not find package '{}' after installing it. This is usually an indication that the dependency has a broken CMakeLists.txt.", cdep.dep.name)};
		}
		write_successful_install_meta_file(ctx, state.dirs, cdep, cmake_config);
	}
}

auto configure_build_install(context* ctx, const dip::state& state, const dip::collected_dep& cdep, std::string_view cmake_config) -> void {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, cdep.version);
	const auto bld_dir_path = make_bld_dir_path(ctx, state.dirs, cdep.version, cmake_config);
	configure(ctx, state, cdep, src_dir_path, bld_dir_path, cdep.cmake_options, cmake_config);
	build(ctx, state, cdep, bld_dir_path, cmake_config);
	install(ctx, state, cdep, bld_dir_path, cmake_config);
}

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
auto init_collector_result(context* ctx) -> dip::collector_result {
	return {
		.just_acquired_deps = std::pmr::vector<std::pmr::string>{ctx->mem},
		.collected_deps     = dip::collected_deps{ctx->mem}
	};
}

[[nodiscard]]
auto get_parent(context* ctx, const dip::ancestry& ancestry) -> std::pmr::string {
	if (ancestry.empty()) { return std::pmr::string{ctx->mem}; }
	else                  { return ancestry.back(); }
}

auto install(context* ctx, const dip::state& state, const dip::collector_result& collector_result, dip::installer* installer, const collected_dep& cdep) -> void {
	for (const auto cmake_config : installer->work_to_do.cmake_configs) {
		if (to_install(ctx, state, collector_result, *installer, cdep, cmake_config)) {
			configure_build_install(ctx, state, cdep, cmake_config);
			installer->install_log.push_back(cdep.dep.name);
		}
	}
	ctx->log->dep_task(decorate(ctx, cdep), "Ready");
}

[[nodiscard]] auto run_collector(context* ctx, const dip::state& state, const yml_registry& registry, const dip::collector& collector) -> collector_result;

[[nodiscard]]
auto set_commit(dip::origin origin, std::string_view new_commit) -> dip::origin {
	if (const auto git_repo    = std::get_if<origin_git_repo>(&origin))           { git_repo->commit = new_commit; return origin; }
	if (const auto git_tracked = std::get_if<origin_git_tracked_branch>(&origin)) { git_tracked->commit = new_commit; return origin; }
	throw std::runtime_error("Cannot set commit for origin that is not a git repository.");
}

auto apply(dip::dep* dep, const commit_update& update) -> void {
	dep->origin = set_commit(std::move(dep->origin), update.new_commit);
}

auto apply(yml_registry* registry, const commit_update& update) -> void {
	apply(get_dep(registry, update.name), update);
}

auto apply(yml_registry* registry, const md5_update& update) -> void {
	auto dep = get_dep(registry, update.name);
	assert (std::holds_alternative<origin_url>(dep->origin));
	std::get<origin_url>(dep->origin).md5 = update.new_md5;
}

auto apply(yml_registry* registry, const registry_update& update) -> void {
	for (const auto& upd : update.commit_updates) { apply(registry, upd); }
	for (const auto& upd : update.md5_updates)    { apply(registry, upd); }
}

auto erase_existing_deps(collector_work_to_do* work_to_do, std::span<const collected_dep> existing_cdeps) -> void {
	auto fn_is_existing = [&existing_cdeps](std::string_view name) {
		auto fn_name_is = [name](const collected_dep& cdep) { return cdep.dep.name == name; };
		return std::ranges::any_of(existing_cdeps, fn_name_is);
	};
	auto& list = work_to_do->deps;
	list.erase(std::remove_if(list.begin(), list.end(), fn_is_existing), list.end());
}

[[nodiscard]]
auto subcollect(context* ctx, const dip::state& state, const ancestry& parent_ancestry, std::span<const collected_dep> existing_cdeps, const collected_dep& cdep) -> subcollector_result {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, cdep.version);
	if (const auto dip_dir = find_dip_dir(ctx, src_dir_path)) {
		ctx->log->detail(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DIP, dip_dir->string()));
		const auto registry_override = make_registry_override_path(state.dirs, cdep.version);
		const auto dep_settings      = read_project_settings_yml(ctx, *dip_dir);
		const auto registry_path     = get_dep_registry_to_use(ctx, *dip_dir, registry_override);
		auto ancestry  = make_ancestry(ctx, parent_ancestry, cdep.dep.name);
		auto registry  = read_registry_yml(ctx, registry_path);
		auto collector = init_collector(ctx, std::move(ancestry), registry, state.work_requested);
		erase_existing_deps(&collector.work_to_do, existing_cdeps);
		auto collector_result = run_collector(ctx, state, registry, collector);
		auto subcollector_result = dip::subcollector_result{
			.just_acquired_deps = std::move(collector_result.just_acquired_deps),
			// If consumer didn't specify package names, use the
			// package names specified in the dependency settings.
			.package_names      = cdep.dep.package_names.empty() ? dep_settings.package_names : cdep.dep.package_names,
			.subdeps            = get_dep_names(ctx, registry),
			.collected_deps     = std::move(collector_result.collected_deps),
		};
		apply(&registry, collector_result.registry_update);
		save_to(ctx, registry, registry_override);
		return subcollector_result;
	}
	return {
		.package_names = cdep.dep.package_names
	};
}

[[nodiscard]]
auto is_dependency_of(const collected_dep& a, const collected_dep& b) -> bool {
	return std::ranges::find(b.dependencies, a.dep.name) != std::cend(b.dependencies);
}

[[nodiscard]]
auto dependency_graph_sort(const collected_dep& a, const collected_dep& b) -> bool {
	if (is_dependency_of(a, b)) { return true; }
	if (is_dependency_of(b, a)) { return false; }
	return a.dep.name < b.dep.name;
}

[[nodiscard]]
auto run_collector(context* ctx, const dip::state& state, const yml_registry& registry, const dip::collector& collector) -> collector_result {
	auto result = collector_result{};
	for (const auto& name : collector.work_to_do.deps) {
		ctx->log->detail(pmr_format(ctx, "Collecting dependency '{}'", name));
		print_and_clear_log(ctx);
		auto dep = get_dep(registry, name);
		if (to_track(ctx, collector, dep)) {
			if (auto update = update_track_commit(ctx, dep, state, collector)) {
				apply(&dep, *update);
				result.registry_update.commit_updates.push_back(std::move(*update));
			}
		}
		const auto cmake_options     = get_cmake_options_list(ctx, os::get_platform(), state.project_settings.cmake_options, dep.cmake_options);
		const auto version           = make_version_string(ctx, dep.origin, cmake_options);
		const auto registry_override = make_registry_override_path(state.dirs, version);
		ctx->log->detail(pmr_format(ctx, "version: '{}'", version));
		if (to_acquire(ctx, state, collector, dep.name, version)) {
			auto acquire_result = acquire(ctx, state, collector, dep, version);
			if (acquire_result.md5_update) {
				result.registry_update.md5_updates.push_back(std::move(*acquire_result.md5_update));
			}
			result.just_acquired_deps.push_back(to_pmr_string(ctx, name));
			remove_if_exists(registry_override);
		}
		auto cdep = collected_dep {
			.dep           = dep,
			.ancestry      = collector.ancestry,
			.version       = version,
			.cmake_options = cmake_options,
		};
		result.collected_deps.push_back(std::move(cdep));
	}
	for (auto& cdep : result.collected_deps) {
		auto subcollect_result = subcollect(ctx, state, collector.ancestry, result.collected_deps, cdep);
		cdep.package_names = std::move(subcollect_result.package_names);
		cdep.dependencies  = std::move(subcollect_result.subdeps);
		std::ranges::copy(subcollect_result.just_acquired_deps, std::back_inserter(result.just_acquired_deps));
		std::ranges::copy(subcollect_result.collected_deps, std::back_inserter(result.collected_deps));
	}
	return result;
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
auto make_self_dep(context* ctx, const yml_project_settings& settings, const std::filesystem::path& project_dir) -> dep {
	if (settings.name.empty()) {
		throw std::runtime_error{"Can't self-install this project because it has no project name."};
	}
	return dip::dep{
		.name          = to_pmr_string(ctx, settings.name),
		.package_names = settings.package_names,
		.origin        = project_dir,
	};
}

[[nodiscard]]
auto get_initial_registry(context* ctx, const yml_project_settings& settings, const std::filesystem::path& project_dir, const std::filesystem::path& dip_dir, bool install_self) -> yml_registry {
	if (install_self) {
		auto registry = yml_registry{
			.deps = std::pmr::vector<dep>{ctx->mem}
		};
		registry.deps.push_back(make_self_dep(ctx, settings, project_dir));
		return registry;
	}
	else {
		return read_registry_yml(ctx, dip_dir / FILENAME_REGISTRY_YML);
	}
}

[[nodiscard]]
auto get_all_dep_versions_in_meta_dir(context* ctx, const std::filesystem::path& meta_dir) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (std::filesystem::exists(meta_dir)) {
		for (const auto& entry : std::filesystem::directory_iterator{meta_dir}) {
			if (entry.is_regular_file()) {
				const auto meta = read_meta_yml(ctx, entry.path());
				list.push_back(meta.version);
			}
		}
	}
	return list;
}

[[nodiscard]]
auto get_all_dep_versions_in_install_dir(context* ctx, const std::filesystem::path& install_dir) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (std::filesystem::exists(install_dir)) {
		for (const auto& entry : std::filesystem::directory_iterator{install_dir}) {
			if (entry.is_directory()) {
				const auto cmake_config = to_string(ctx, entry.path().filename());
				const auto meta_dir     = entry.path() / "meta";
				list.append_range(get_all_dep_versions_in_meta_dir(ctx, meta_dir));
			}
		}
	}
	return list;
}

[[nodiscard]]
auto get_root_installed_versions(context* ctx, const std::filesystem::path& root) -> std::pmr::vector<std::pmr::string> {
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	if (std::filesystem::exists(root)) {
		for (const auto& entry : std::filesystem::directory_iterator{root}) {
			if (entry.is_directory()) {
				const auto project_name = to_string(ctx, entry.path().filename());
				const auto install_dir  = entry.path() / "install";
				list.append_range(get_all_dep_versions_in_install_dir(ctx, install_dir));
			}
		}
	}
	return list;
}

[[nodiscard]]
auto get_versions_to_preserve(context* ctx, std::span<const std::filesystem::path> roots_to_preserve) -> std::pmr::vector<std::pmr::string> {
	auto fn_get_root_versions = [ctx](const std::filesystem::path& root) { return get_root_installed_versions(ctx, root); };
	auto view = roots_to_preserve
		| std::views::transform(fn_get_root_versions)
		| std::views::join;
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	std::ranges::copy(view, std::back_inserter(list));
	return sort_and_remove_duplicates(ctx, list);
}

[[nodiscard]]
auto do_cache_clean(context* ctx, const dip::args& args) -> int {
	const auto sys_cache_dir        = os::get_system_cache_dir();
	const auto cache_dir            = get_cache_dir(sys_cache_dir, args);
	const auto root_dirs            = get_root_dirs(ctx, sys_cache_dir, args);
	const auto versions_to_preserve = get_versions_to_preserve(ctx, root_dirs);
	for (const auto& entry : std::filesystem::directory_iterator{cache_dir}) {
		if (entry.is_directory()) {
			const auto version = to_string(ctx, entry.path().filename());
			if (!std::ranges::binary_search(versions_to_preserve, version)) {
				ctx->log->info(pmr_format(ctx, "Removing cache directory '{}'", entry.path().string()));
				print_and_clear_log(ctx);
				std::filesystem::remove_all(entry.path());
			}
		}
	}
	return exit_success(ctx);
}

[[nodiscard]]
auto do_dip(context* ctx, const dip::args& args) -> int {
	if (const auto reqs = check_requirements(ctx)) {
		if (const auto dip_dir = find_dip_dir(ctx, args.project_dir.v)) {
			const auto no_ancestry   = dip::ancestry{ctx->mem};
			const auto save_registry = !args.install_self.v;
			auto state               = init_state(ctx, args, *reqs, *dip_dir);
			auto registry            = get_initial_registry(ctx, state.project_settings, args.project_dir.v, *dip_dir, args.install_self.v);
			auto collector           = init_collector(ctx, no_ancestry, registry, state.work_requested);
			auto collector_result    = run_collector(ctx, state, registry, collector);
			std::ranges::sort(collector_result.collected_deps, dependency_graph_sort);
			if (save_registry) {
				apply(&registry, collector_result.registry_update);
				save_to(ctx, registry, *dip_dir / FILENAME_REGISTRY_YML);
			}
			auto installer = init_installer(ctx, state.project_settings, state.work_requested);
			run_installer(ctx, state, collector_result, &installer);
			print_cmake_prefix_help(ctx, state.dirs, installer.work_to_do.cmake_configs);
			return exit_success(ctx);
		}
		ctx->log->error(pmr_format(ctx, "No '{}' directory found in project directory '{}'", DIR_PROJECT_DIP, args.project_dir.v.string()));
		return exit_failure(ctx);
	}
	return exit_failure(ctx);
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (args.cache_clean.v) { return do_cache_clean(ctx, args); }
	else                    { return do_dip(ctx, args); }
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
