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

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

struct work_to_do {
	std::pmr::vector<std::pmr::string> cfgs;
	std::pmr::vector<std::pmr::string> process;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
	std::optional<dip::dep> self_to_install;
};

struct work_requested {
	std::pmr::vector<std::pmr::string> cfg;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
};

struct work_done {
	bool at_least_one_dep_was_installed = false;
};

struct state {
	dip::dirs dirs;
	dip::prog_paths prog_paths;
	std::pmr::vector<std::pmr::string> ancestry;
	yml_project_settings project_settings;
	yml_registry registry;
	dip::work_requested work_requested;
	dip::work_to_do work_to_do;
	dip::work_done work_done;
	bool verbose = false;
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
auto fn_dep_name_is(std::string_view name) {
	return [name](const dip::dep& dep) { return dep.name == name; };
}

[[nodiscard]]
auto get_dep_names(context* ctx, const yml_registry& registry) -> std::pmr::vector<std::pmr::string> {
	auto deps = std::pmr::vector<std::pmr::string>{ctx->mem};
	std::ranges::transform(registry.deps, std::back_inserter(deps), &dep::name);
	return deps;
}

[[nodiscard]]
auto get_position_in_registry(const yml_registry& registry, std::string_view dep_name) -> size_t {
	if (const auto pos = std::ranges::find_if(registry.deps, fn_dep_name_is(dep_name)); pos != registry.deps.end()) {
		return std::distance(registry.deps.begin(), pos);
	}
	throw std::runtime_error(std::format("Dependency '{}' not found in registry.", dep_name));
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
auto expand_track(context* ctx, const yml_registry& registry, std::pmr::vector<std::pmr::string> list) -> std::pmr::vector<std::pmr::string> {
	if (list.size() == 1 && list.front() == ARG_TRACK_ALL_VALUE) {
		return get_dep_names(ctx, registry);
	}
	return list;
}

[[nodiscard]]
auto get_cfgs_to_process(context* ctx, const yml_project_settings& settings, const dip::work_requested& work_requested) -> std::pmr::vector<std::pmr::string> {
	if (!work_requested.cfg.empty()) {
		return work_requested.cfg;
	}
	const auto fn_get_cfg_name = [](const dip::cfg& cfg) { return cfg.name; };
	auto list = std::pmr::vector<std::pmr::string>{ctx->mem};
	std::ranges::transform(settings.cfgs, std::back_inserter(list), fn_get_cfg_name);
	return list;
}

[[nodiscard]]
auto get_work_to_do(context* ctx, const yml_project_settings& settings, const yml_registry& registry, const dip::work_requested& work_requested) -> work_to_do {
	auto track     = expand_track(ctx, registry, work_requested.track);
	auto reacquire = work_requested.reacquire;
	return work_to_do{
		.cfgs            = get_cfgs_to_process(ctx, settings, work_requested),
		.process         = sort_deps_into_processing_order(get_dep_names(ctx, registry), registry),
		.track           = sort_and_remove_duplicates(ctx, track),
		.reacquire       = sort_and_remove_duplicates(ctx, reacquire)
	};
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
		.project = args.project_dir.v,
	};
}

auto get_work_requested(const dip::args& args) -> dip::work_requested {
	return dip::work_requested{
		.cfg          = args.cfg.v,
		.track        = args.track.v,
		.reacquire    = args.reacquire.v,
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
auto make_self_dep(const dip::state& state) -> dep {
	const auto name = state.project_settings.name;
	if (name.empty()) {
		throw std::runtime_error{"Can't self-install this project because settings.yml doesn't contain a 'name' key."};
	}
	return dip::dep{
		.name   = name,
		.origin = state.dirs.project,
	};
}

[[nodiscard]]
auto make_ancestry(context* ctx, std::pmr::vector<std::pmr::string> parent_ancestry, std::string_view dep_name) -> std::pmr::vector<std::pmr::string> {
	auto list = std::move(parent_ancestry);
	list.push_back(to_string(ctx, dep_name));
	return list;
}

auto init_state(context* ctx, dip::state* state, const dip::args& args, const requirements& reqs, const std::filesystem::path& dip_dir) -> void {
	state->project_settings = read_project_settings_yml(ctx, dip_dir);
	state->dirs             = get_dirs(ctx, args, state->project_settings.name);
	state->work_requested   = get_work_requested(args);
	state->verbose          = args.verbose.v;
	state->prog_paths       = get_prog_paths(reqs);
	state->registry         = read_registry_yml(ctx, dip_dir / FILENAME_REGISTRY_YML);
	state->work_to_do       = get_work_to_do(ctx, state->project_settings, state->registry, state->work_requested);
	if (args.install_self.v) {
		state->work_to_do.self_to_install = make_self_dep(*state);
	}
}

auto make_dependency_subprocessing_state(context* ctx, dip::state parent_state, const dip::dep& dep, const std::filesystem::path& src_dir, const std::filesystem::path& registry_path) -> dip::state {
	// Start by inheriting everything from the parent.
	auto state = std::move(parent_state);
	// Then override stuff.
	state.dirs.project               = src_dir;
	state.ancestry                   = make_ancestry(ctx, state.ancestry, dep.name);
	state.registry                   = read_registry_yml(ctx, registry_path);
	state.work_to_do                 = get_work_to_do(ctx, state.project_settings, state.registry, state.work_requested);
	state.work_to_do.self_to_install = dep;
	return state;
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
auto user_requested_reacquire(const dip::state& state, std::string_view name) -> bool {
	return std::ranges::binary_search(state.work_to_do.reacquire, name);
}

[[nodiscard]]
auto user_requested_track(context*, const dip::state& state, std::string_view name) -> bool {
	return std::ranges::binary_search(state.work_to_do.track, name);
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
auto to_acquire(context* ctx, const dip::state& state, std::string_view dep_name, std::string_view version) -> bool {
	return
		!have_source_code(ctx, state, dep_name, version) ||
		user_requested_reacquire(state, dep_name);
}

[[nodiscard]]
auto to_track(context* ctx, const dip::state& state, const dip::dep& dep) -> bool {
	const auto empty_track_commit = has_empty_track_commit(dep);
	const auto user_requested     = user_requested_track(ctx, state, dep.name);
	const auto origin_is_track    = std::holds_alternative<origin_git_tracked_branch>(dep.origin);
	return empty_track_commit || (user_requested && origin_is_track);
}

[[nodiscard]]
auto to_install(context* ctx, const dip::state& state, const dip::dep& dep, const dip::cfg& cfg) -> bool {
	return
		!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, dep.name, cfg);
}

[[nodiscard]]
auto to_install(context* ctx, const dip::state& state, const dip::dep& dep) -> bool {
	for (const auto cfg_name : state.work_to_do.cfgs) {
		const auto& cfg = get_cfg(state.project_settings, cfg_name);
		if (to_install(ctx, state, dep, cfg)) {
			return true;
		}
	}
	return false;
}

[[nodiscard]]
auto get_dep(dip::state* state, std::string_view name) -> dip::dep* {
	if (auto pos = std::ranges::find_if(state->registry.deps, fn_dep_name_is(name)); pos != std::cend(state->registry.deps)) {
		return &*pos;
	}
	throw std::runtime_error(std::format("Dependency '{}' not found in registry.", name));
}

[[nodiscard]]
auto make_ancestry_string(context* ctx, const std::pmr::vector<std::pmr::string>& ancestry) -> std::pmr::string {
	if (ancestry.empty()) { return std::pmr::string{ctx->mem}; }
	return join<std::pmr::string>(ctx, ancestry, " -> ");
}

[[nodiscard]]
auto decorate(context* ctx, const dip::state& state, std::string_view dep_name) -> std::pmr::string {
	if (state.ancestry.empty()) { return to_pmr_string(ctx, dep_name); }
	else                        { return pmr_format(ctx, "{} -> {}", make_ancestry_string(ctx, state.ancestry), dep_name); }
}

[[nodiscard]]
auto update_track_commit(context* ctx, dip::dep* dep, const dip::state& state, origin_git_tracked_branch git) -> bool {
	ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Fetching latest commit from '{}'", git.url));
	print_and_clear_log(ctx);
	const auto new_hash = get_latest_git_commit_hash(ctx, state.prog_paths, git.url, git.branch);
	ctx->log->detail(pmr_format(ctx, "latest commit is '{}'", new_hash));
	if (new_hash != git.commit) {
		git.commit = new_hash;
		dep->origin = git;
		ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Updated commit to '{}'", git.commit));
		return true;
	}
	ctx->log->detail("commit is already at latest");
	return false;
}

auto update_track_commit(context* ctx, dip::dep* dep, const dip::state& state) -> bool {
	assert (std::holds_alternative<origin_git_tracked_branch>(dep->origin));
	return update_track_commit(ctx, dep, state, std::get<origin_git_tracked_branch>(dep->origin));
}

auto extract_to(context* ctx, const dip::state& state, const dip::dep& dep, const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir_path) -> void {
	ctx->log->dep_task(decorate(ctx, state, dep.name), pmr_format(ctx, "Extracting '{}'", archive_path.filename().string()));
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

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, std::string_view version, const std::filesystem::path& origin) -> void {
	ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Copying source code from '{}'", origin.string()));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	const auto copy_options =
		std::filesystem::copy_options::recursive |
		std::filesystem::copy_options::overwrite_existing;
	std::filesystem::copy(origin, src_dir_path, copy_options);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, std::string_view version, const origin_git_repo& origin) -> void {
	ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, std::string_view version, const origin_git_tracked_branch& origin) -> void {
	ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.commit));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	git_clone(ctx, state.prog_paths, origin.url, origin.commit, src_dir_path);
}

auto acquire_src_from_origin(context* ctx, dip::dep* dep, const dip::state& state, std::string_view version, const origin_url& origin) -> void {
	ctx->log->dep_task(decorate(ctx, state, dep->name), pmr_format(ctx, "Downloading source code from '{}'", origin.url));
	print_and_clear_log(ctx);
	const auto dl_dir_path     = make_dl_dir_path(ctx, state.dirs, version);
	const auto src_dir_path    = make_src_dir_path(ctx, state.dirs, version);
	const auto downloaded_file = download_file(ctx, state.prog_paths, origin.url, dl_dir_path);
	md5_check_or_update(ctx, dep, downloaded_file, origin);
	extract_to(ctx, state, *dep, downloaded_file, src_dir_path);
}

auto acquire(context* ctx, dip::dep* dep, const dip::state& state, std::string_view version) -> void {
	std::visit([ctx, dep, &state, version](const auto& origin) { acquire_src_from_origin(ctx, dep, state, version, origin); }, dep->origin);
}

auto configure(context* ctx, const dip::state& state, const dip::dep& dep, const std::filesystem::path& src_dir_path, const std::filesystem::path& bld_dir_path, std::span<const std::pmr::string> cmake_options_list, const dip::cfg& cfg) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, state, dep.name), to_pmr_string(ctx, cfg.name), pmr_format(ctx, "Configure"));
	print_and_clear_log(ctx);
	const auto install_prefix_path = make_install_prefix_path(state.dirs, cfg.name);
	const auto cmakelists          = find_file_in_dir(ctx, src_dir_path, "CMakeLists.txt");
	if (!cmakelists) {
		throw std::runtime_error(std::format("No CMakeLists.txt found in source directory '{}'", src_dir_path.string()));
	}
	const auto cmake_options_string = get_cmake_options_string(ctx, cmake_options_list);
	cmake_configure(ctx, state.prog_paths, install_prefix_path, cmakelists->parent_path(), bld_dir_path, cfg.cmake_config, cmake_options_string);
}

auto build(context* ctx, const dip::state& state, const dip::dep& dep, const std::filesystem::path& bld_dir_path, const dip::cfg& cfg) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, state, dep.name), to_pmr_string(ctx, cfg.name), pmr_format(ctx, "Build"));
	print_and_clear_log(ctx);
	cmake_build(ctx, state.prog_paths, bld_dir_path, cfg.cmake_config);
}

auto install(context* ctx, const dip::state& state, const dip::dep& dep, const std::filesystem::path& bld_dir_path, const dip::cfg& cfg) -> void {
	ctx->log->dep_cfg_task(decorate(ctx, state, dep.name), to_pmr_string(ctx, cfg.name), pmr_format(ctx, "Install"));
	print_and_clear_log(ctx);
	cmake_install(ctx, state.prog_paths, bld_dir_path, cfg.cmake_config);
	if (!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, dep.name, cfg)) {
		throw std::runtime_error{std::format("CMake could still not find package '{}' after installing it.", dep.name)};
	}
}

auto configure_build_install(context* ctx, const dip::state& state, const dip::dep& dep, std::string_view version, std::span<const std::pmr::string> cmake_options_list, const dip::cfg& cfg) -> void {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	const auto bld_dir_path = make_bld_dir_path(ctx, state.dirs, version, cfg.name);
	configure(ctx, state, dep, src_dir_path, bld_dir_path, cmake_options_list, cfg);
	build(ctx, state, dep, bld_dir_path, cfg);
	install(ctx, state, dep, bld_dir_path, cfg);
}

auto do_work(context* ctx, dip::state* state) -> void;

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
auto run_dip_on(context* ctx, const dip::state& state, const dip::dep& dep, std::string_view version, const std::filesystem::path& registry_override) -> bool {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, version);
	if (const auto dip_dir = find_dip_dir(ctx, src_dir_path)) {
		ctx->log->detail(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DIP, dip_dir->string()));
		const auto registry_path = get_dep_registry_to_use(ctx, *dip_dir, registry_override);
		auto dep_state = make_dependency_subprocessing_state(ctx, state, dep, src_dir_path, registry_path);
		do_work(ctx, &dep_state);
		save_to(ctx, dep_state.registry, registry_override);
		return true;
	}
	return false;
}

auto do_process(context* ctx, dip::state* state, std::string_view name) -> void {
	auto dep = get_dep(state, name);
	if (to_track(ctx, *state, *dep)) {
		update_track_commit(ctx, dep, *state);
	}
	const auto cmake_options     = get_cmake_options_list(ctx, os::get_platform(), state->project_settings.cmake_options, dep->cmake_options);
	const auto version           = make_version_string(ctx, dep->origin, cmake_options);
	const auto registry_override = make_registry_override_path(state->dirs, version);
	ctx->log->detail(pmr_format(ctx, "version: '{}'", version));
	if (to_acquire(ctx, *state, dep->name, version)) {
		acquire(ctx, dep, *state, version);
		remove_if_exists(registry_override);
	}
	if (run_dip_on(ctx, *state, *dep, version, registry_override)) {
		ctx->log->dep_task(decorate(ctx, *state, dep->name), "Ready");
		return;
	}
	for (const auto cfg_name : state->work_to_do.cfgs) {
		const auto& cfg = get_cfg(state->project_settings, cfg_name);
		if (to_install(ctx, *state, *dep, cfg)) {
			configure_build_install(ctx, *state, *dep, version, cmake_options, cfg);
			state->work_done.at_least_one_dep_was_installed = true;
		}
	}
	ctx->log->dep_task(decorate(ctx, *state, dep->name), "Ready");
}

auto install_self(context* ctx, const dip::state& state, const dep& self) -> void {
	for (const auto cfg_name : state.work_to_do.cfgs) {
		const auto& cfg = get_cfg(state.project_settings, cfg_name);
		const auto cmake_options  = get_cmake_options_list(ctx, os::get_platform(), state.project_settings.cmake_options, self.cmake_options);
		const auto version        = make_version_string(ctx, self.origin, cmake_options);
		const auto package_found  = cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, self.name, cfg);
		const auto deps_installed = state.work_done.at_least_one_dep_was_installed;
		if (deps_installed || !package_found) {
			configure_build_install(ctx, state, self, version, cmake_options, cfg);
		}
	}
}

auto do_work(context* ctx, dip::state* state) -> void {
	for (const auto& name : state->work_to_do.process) {
		print_and_clear_log(ctx);
		do_process(ctx, state, name);
	}
	if (state->work_to_do.self_to_install) {
		install_self(ctx, *state, *state->work_to_do.self_to_install);
	}
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (const auto reqs = check_requirements(ctx)) {
		if (const auto dip_dir = find_dip_dir(ctx, args.project_dir.v)) {
			auto state = dip::state{};
			init_state(ctx, &state, args, *reqs, *dip_dir);
			do_work(ctx, &state);
			save_to(ctx, state.registry, *dip_dir / FILENAME_REGISTRY_YML);
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
