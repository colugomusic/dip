#include "args.hpp"
#include "cmake.hpp"
#include "colors.hpp"
#include "list-util.hpp"
#include "git.hpp"
#include "requirements.hpp"
#include "wget.hpp"
#include "yaml.hpp"
#include "zip.hpp"
#include <string>
#include <vector>

namespace dip {

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

struct file_paths {
	std::filesystem::path project_settings_yml;
};

struct work_to_do {
	std::pmr::vector<std::pmr::string> cfgs;
	std::pmr::vector<std::pmr::string> process;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
};

struct work_requested {
	std::pmr::vector<std::pmr::string> cfg;
	std::pmr::vector<std::pmr::string> track;
	std::pmr::vector<std::pmr::string> reacquire;
};

struct state {
	dip::dirs dirs;
	dip::file_paths file_paths;
	dip::prog_paths prog_paths;
	yml_project_settings project_settings;
	yml_registry registry;
	dip::work_requested work_requested;
	dip::work_to_do work_to_do;
	bool install_self = false;
	bool verbose      = false;
};

auto print(const context* ctx, log_dep_task v)     { if (ctx->print_options.dep_tasks) { std::cout << pmr_format(ctx, "{}{}{}: {}\n", colors::dep, v.dep, colors::reset, v.task); } }
auto print(const context* ctx, log_dep_cfg_task v) { if (ctx->print_options.dep_tasks) { std::cout << pmr_format(ctx, "{}{}{}: {}{}{}: {}\n", colors::dep, v.dep, colors::reset, colors::cfg, v.cfg, colors::reset, v.task); } }
auto print(const context* ctx, log_detail v)       { if (ctx->print_options.detail)    { std::cout << pmr_format(ctx, "{}{}{}\n", colors::detail, v.v, colors::reset); } }
auto print(const context* ctx, log_error v)        { if (ctx->print_options.errors)    { std::cout << pmr_format(ctx, "\n{}{}{}\n", colors::error, v.v, colors::reset); } }
auto print(const context* ctx, log_info v)         { if (ctx->print_options.info)      { std::cout << pmr_format(ctx, "{}{}{}\n", colors::info, v.v, colors::reset); } }
auto print(const context* ctx, log_warn v)         { if (ctx->print_options.warnings)  { std::cout << pmr_format(ctx, "{}{}{}\n", colors::warning, v.v, colors::reset); } }

[[nodiscard]] auto fn_print_dep_task(const context* ctx)     { return [ctx](log_dep_task v)     { print(ctx, v); }; }
[[nodiscard]] auto fn_print_dep_cfg_task(const context* ctx) { return [ctx](log_dep_cfg_task v) { print(ctx, v); }; }
[[nodiscard]] auto fn_print_detail(const context* ctx)       { return [ctx](log_detail v)       { print(ctx, v); }; }
[[nodiscard]] auto fn_print_error(const context* ctx)        { return [ctx](log_error v)        { print(ctx, v); }; }
[[nodiscard]] auto fn_print_info(const context* ctx)         { return [ctx](log_info v)         { print(ctx, v); }; }
[[nodiscard]] auto fn_print_warning(const context* ctx)      { return [ctx](log_warn v)         { print(ctx, v); }; }

auto print_and_clear_log(const context* ctx) -> void {
	auto fns = logger_fns{
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
auto find_dip_dir(context* ctx, const std::filesystem::path& project_dir) -> std::filesystem::path {
	const auto dip_dir = project_dir / DIR_PROJECT_DIP;
	if (std::filesystem::exists(dip_dir)) {
		ctx->log->info(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DIP, dip_dir.string()));
		return dip_dir;
	}
	const auto dope_dir = project_dir / DIR_PROJECT_DOPE;
	if (std::filesystem::exists(dope_dir)) {
		ctx->log->info(pmr_format(ctx, "Found '{}' directory at '{}'", DIR_PROJECT_DOPE, dope_dir.string()));
		return dope_dir;
	}
	ctx->log->info(pmr_format(ctx, "No '{}' or '{}' directory found in project directory '{}', so using default: '{}'", DIR_PROJECT_DIP, DIR_PROJECT_DOPE, project_dir.string(), dip_dir.string()));
	return dip_dir;
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
		.cfgs      = get_cfgs_to_process(ctx, settings, work_requested),
		.process   = sort_deps_into_processing_order(get_dep_names(ctx, registry), registry),
		.track     = sort_and_remove_duplicates(ctx, track),
		.reacquire = sort_and_remove_duplicates(ctx, reacquire)
	};
}

[[nodiscard]]
auto make_default_project_settings_yml(const std::filesystem::path& dip_dir) -> yml_project_settings {
	return yml_project_settings{
		.registry_path = make_default_registry_yml_file_path(dip_dir)
	};
}

auto print_info_about_default_directories(context* ctx, const dip::args& args, const std::filesystem::path& sys_cache_dir, const std::filesystem::path& cache, const std::filesystem::path& root) -> void {
	const auto arg_cache = args.cache.v;
	const auto arg_root  = args.root.v;
	if (arg_cache && arg_root) {
		return;
	}
	ctx->log->info(pmr_format(ctx, "System cache folder is: '{}'", sys_cache_dir.string()));
	if (!arg_cache && !arg_root) {
		ctx->log->info(pmr_format(ctx,
			"Using these default directory paths because you didn't specify them:\n"
			" - cache: '{}' (override with --cache path/to/cache)\n"
			" - root:  '{}' (override with --root path/to/root)",
			cache.string(), root.string()
		));
		return;
	}
	if (!arg_cache) {
		ctx->log->info(pmr_format(ctx,
			"Using '{}' as cache because you didn't specify one.\n"
			"If you're not happy with this then specify a cache with --cache \"path/to/cache\"",
			cache.string()));
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

auto get_dirs(context* ctx, const dip::args& args) -> dip::dirs {
	const auto sys_cache_dir = os::get_system_cache_dir();
	auto cache = args.cache.v.value_or(sys_cache_dir / "dip-cache");
	auto root  = args.root.v.value_or(sys_cache_dir / "dip-root");
	print_info_about_default_directories(ctx, args, sys_cache_dir, cache, root);
	return dip::dirs{
		.cache   = cache,
		.root    = root,
		.project = args.project_dir.v,
		.dip     = find_dip_dir(ctx, args.project_dir.v)
	};
}

auto get_work_requested(const dip::args& args) -> dip::work_requested {
	return dip::work_requested{
		.cfg       = args.cfg.v,
		.track     = args.track.v,
		.reacquire = args.reacquire.v
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

auto init_state(context* ctx, dip::state* state, const dip::args& args, const requirements& reqs) -> void {
	state->dirs                            = get_dirs(ctx, args);
	state->file_paths.project_settings_yml = state->dirs.dip / FILENAME_SETTINGS_YML;
	state->project_settings                = read_project_settings_yml(ctx, state->dirs.dip, state->file_paths.project_settings_yml).value_or(make_default_project_settings_yml(state->dirs.dip));
	state->work_requested                  = get_work_requested(args);
	state->install_self                    = args.install_self.v;
	state->verbose                         = args.verbose.v;
	state->prog_paths                      = get_prog_paths(reqs);
	state->registry                        = read_registry_yml(ctx, state->project_settings.registry_path);
	state->work_to_do                      = get_work_to_do(ctx, state->project_settings, state->registry, state->work_requested);
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
auto has_empty_track_tag(const dip::dep& dep) -> bool {
	if (dep.track) {
		if (const auto git = std::get_if<origin_git_repo>(&dep.origin)) {
			return git->tag.empty();
		}
	}
	return false;
}

[[nodiscard]]
auto is_git_dep(const dip::dep& dep) -> bool {
	return std::holds_alternative<origin_git_repo>(dep.origin);
}

[[nodiscard]]
auto user_requested_reacquire(const dip::state& state, std::string_view name) -> bool {
	return std::ranges::binary_search(state.work_to_do.reacquire, name);
}

[[nodiscard]]
auto user_requested_track(const dip::state& state, std::string_view name) -> bool {
	return std::ranges::binary_search(state.work_to_do.track, name);
}

[[nodiscard]]
auto have_source_code(context* ctx, const dip::state& state, const dip::dep& dep) -> bool {
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, dep);
	const auto cmakelists_path = src_dir_path / "CMakeLists.txt";
	const auto have_cmakelists = std::filesystem::exists(cmakelists_path);
	if (have_cmakelists) {
		ctx->log->detail(pmr_format(ctx, "Found CMakeLists.txt for '{}' at '{}'", dep.name, cmakelists_path.string()));
	}
	else {
		ctx->log->detail(pmr_format(ctx, "No CMakeLists.txt found for '{}' at '{}'", dep.name, cmakelists_path.string()));
	}
	return have_cmakelists;
}

[[nodiscard]]
auto to_acquire(context* ctx, const dip::state& state, const dip::dep& dep) -> bool {
	return
		!have_source_code(ctx, state, dep) ||
		user_requested_reacquire(state, dep.name);
}

[[nodiscard]]
auto to_track(const dip::state& state, const dip::dep& dep) -> bool {
	return
		has_empty_track_tag(dep) ||
		user_requested_track(state, dep.name) && is_git_dep(dep);
}

[[nodiscard]]
auto to_install(context* ctx, const dip::state& state, const dip::dep& dep, std::string_view cfg) -> bool {
	return
		!cmake_package_can_be_found(ctx, state.dirs, state.prog_paths, dep, cfg);
}

[[nodiscard]]
auto get_dep(dip::state* state, std::string_view name) -> dip::dep* {
	if (auto pos = std::ranges::find_if(state->registry.deps, fn_dep_name_is(name)); pos != std::cend(state->registry.deps)) {
		return &*pos;
	}
	throw std::runtime_error(std::format("Dependency '{}' not found in registry.", name));
}

auto update_track_tag(context* ctx, const dip::prog_paths& progs, dip::dep* dep, origin_git_repo git) -> void {
	ctx->log->dep_task(dep->name, pmr_format(ctx, "Fetching latest commit hash from '{}'", git.url));
	print_and_clear_log(ctx);
	const auto new_hash = get_latest_git_commit_hash(ctx, progs, git.url);
	ctx->log->detail(pmr_format(ctx, "latest commit is '{}'", new_hash));
	git.tag = new_hash;
	dep->origin = git;
	dep->track  = true;
	ctx->log->dep_task(dep->name, pmr_format(ctx, "Updated tag to '{}'", git.tag));
}

auto update_track_tag(context* ctx, const dip::state& state, dip::dep* dep) -> void {
	assert (std::holds_alternative<origin_git_repo>(dep->origin));
	update_track_tag(ctx, state.prog_paths, dep, std::get<origin_git_repo>(dep->origin));
}

auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::dep& dep, const std::filesystem::path& origin) -> void {
	ctx->log->dep_task(dep.name, pmr_format(ctx, "Copying source code from '{}'", origin.string()));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, dep);
	const auto copy_options =
		std::filesystem::copy_options::recursive |
		std::filesystem::copy_options::overwrite_existing;
	std::filesystem::copy(origin, src_dir_path, copy_options);
}

auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::dep& dep, const origin_git_repo& origin) -> void {
	ctx->log->dep_task(dep.name, pmr_format(ctx, "Cloning git repo '{} # {}'", origin.url, origin.tag));
	print_and_clear_log(ctx);
	const auto src_dir_path = make_src_dir_path(ctx, state.dirs, dep);
	// @TODO:
}

auto extract_to(context* ctx, const dip::prog_paths& progs, const dip::dep& dep, const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir_path) -> void {
	ctx->log->dep_task(dep.name, pmr_format(ctx, "Extracting '{}'", archive_path.filename().string()));
	print_and_clear_log(ctx);
	extract_to(ctx, progs, archive_path, dest_dir_path);
}

auto acquire_src_from_origin(context* ctx, const dip::state& state, const dip::dep& dep, const origin_url& origin) -> void {
	ctx->log->dep_task(dep.name, pmr_format(ctx, "Downloading source code from '{}'", origin.url));
	print_and_clear_log(ctx);
	const auto dl_dir_path     = make_dl_dir_path(ctx, state.dirs, dep);
	const auto src_dir_path    = make_src_dir_path(ctx, state.dirs, dep);
	const auto downloaded_file = download_file(ctx, state.prog_paths, origin.url, dl_dir_path);
	// @TODO: check md5
	extract_to(ctx, state.prog_paths, dep, downloaded_file, src_dir_path);
}

auto acquire(context* ctx, const dip::state& state, const dip::dep& dep) -> void {
	std::visit([ctx, &state, &dep](const auto& origin) { acquire_src_from_origin(ctx, state, dep, origin); }, dep.origin);
}

auto install(context* ctx, const dip::state& state, const dip::dep& dep, std::string_view cfg) -> void {
	// @TODO:
}

auto do_process(context* ctx, dip::state* state, std::string_view name) -> void {
	auto dep = get_dep(state, name);
	if (to_track(*state, *dep)) {
		update_track_tag(ctx, *state, dep);
	}
	if (to_acquire(ctx, *state, *dep)) {
		acquire(ctx, *state, *dep);
	}
	for (const auto cfg : state->work_to_do.cfgs) {
		if (to_install(ctx, *state, *dep, cfg)) {
			install(ctx, *state, *dep, cfg);
		}
	}
}

auto do_work(context* ctx, dip::state* state) -> void {
	for (const auto& name : state->work_to_do.process) {
		do_process(ctx, state, name);
	}
	save_to(ctx, state->registry, state->project_settings.registry_path);
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (const auto reqs = check_requirements(ctx)) {
		auto state = dip::state{};
		init_state(ctx, &state, args, *reqs);
		do_work(ctx, &state);
		return exit_success(ctx);
	}
	else {
		return exit_failure(ctx);
	}
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
