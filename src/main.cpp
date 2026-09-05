#include "args.hpp"
#include "colors.hpp"
#include "requirements.hpp"
#include <fkYAML/node.hpp>
#include <fstream>
#include <rang.hpp>
#include <tiny-process-library/process.hpp>
#include <span>
#include <string>
#include <vector>

namespace dip {

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

struct git_repo_url {
	std::pmr::string v;
};

using yml_project_settings_registry = std::variant<std::filesystem::path, git_repo_url>;

struct yml_project_settings {
	yml_project_settings_registry registry;
};

struct yml_registry {
};

struct dirs {
	std::filesystem::path cache;
	std::filesystem::path root;
	std::filesystem::path project;
	std::filesystem::path dip;
};

struct file_paths {
	std::filesystem::path project_settings_yml;
	std::filesystem::path registry_yml;
};

struct prog_paths {
	std::filesystem::path git;
	std::filesystem::path wget;
};

struct state {
	dip::dirs dirs;
	dip::file_paths file_paths;
	dip::prog_paths prog_paths;
	yml_project_settings project_settings;
	yml_registry registry;
	std::pmr::vector<std::pmr::string> track;
	bool install_self = false;
	bool verbose      = false;
};

[[nodiscard]] auto fn_print_error(const context* ctx)   { return [ctx](std::string_view s) { if (ctx->print_options.errors)   { std::cout << pmr_format(ctx, "\n{}{}{}\n", colors::error, s, colors::reset); } }; };
[[nodiscard]] auto fn_print_info(const context* ctx)    { return [ctx](std::string_view s) { if (ctx->print_options.info)     { std::cout << pmr_format(ctx, "{}{}{}\n", colors::info, s, colors::reset); } }; };
[[nodiscard]] auto fn_print_warning(const context* ctx) { return [ctx](std::string_view s) { if (ctx->print_options.warnings) { std::cout << pmr_format(ctx, "{}{}{}\n", colors::warning, s, colors::reset); } }; };

auto print_and_clear_log(const context* ctx) -> void {
	ctx->log->visit(fn_print_error(ctx), fn_print_info(ctx), fn_print_warning(ctx));
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
	fn_print_error(ctx)(what);
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
auto read_file_text(context* ctx, const std::filesystem::path& path) -> std::optional<std::pmr::string> {
	auto file = std::ifstream{path};
	if (!file.is_open()) {
		ctx->log->info(pmr_format(ctx, "Failed to open file at '{}'", path.string()));
		return std::nullopt;
	}
	return std::pmr::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}, ctx->mem};
}

[[nodiscard]]
auto read_string(context* ctx, const fkyaml::node& node, std::string_view key) -> std::optional<std::pmr::string> {
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
auto read_registry(context* ctx, const fkyaml::node& node, const std::filesystem::path& settings_yml_file_path, const std::filesystem::path& default_registry_yml_file_path) -> yml_project_settings_registry {
	if (node.contains(KEY_REGISTRY)) {
		auto value_node = node.at(KEY_REGISTRY);
		if (value_node.is_string()) {
			const auto str = value_node.get_value<std::string>();
			return std::filesystem::path{str};
		}
		if (value_node.is_mapping()) {
			if (value_node.contains(KEY_GIT)) {
				const auto git_node = value_node.at(KEY_GIT);
				if (git_node.is_string()) {
					const auto str = git_node.get_value<std::string>();
					return git_repo_url{std::pmr::string{str.data(), str.size(), ctx->mem}};
				}
				else {
					throw std::runtime_error{std::format("The '{}' key in '{}' must be a string.", KEY_GIT, settings_yml_file_path.string())};
				}
			}
		}
		throw std::runtime_error(std::format("The '{}' key in '{}' must be a string or a mapping containing a '{}' key.", KEY_REGISTRY, settings_yml_file_path.string(), KEY_GIT));
	}
	ctx->log->info(pmr_format(ctx,
		"No '{}' key was found in '{}'.\n"
		"I'm going to assume there's a registry at '{}'.",
		KEY_REGISTRY,
		settings_yml_file_path.string(),
		default_registry_yml_file_path.string()));
	return default_registry_yml_file_path;
}

[[nodiscard]]
auto make_default_registry_yml_file_path(const std::filesystem::path& dip_dir) -> std::filesystem::path {
	return dip_dir / FILENAME_REGISTRY_YML;
}

[[nodiscard]]
auto read_project_settings_yml(context* ctx, const std::filesystem::path& dip_dir, const std::filesystem::path& path) -> std::optional<yml_project_settings> {
	if (std::filesystem::exists(path)) {
		ctx->log->info(pmr_format(ctx, "Reading project settings from '{}'", path.string()));
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = fkyaml::node::deserialize(*text);
			return yml_project_settings {
				.registry = read_registry(ctx, node, path, make_default_registry_yml_file_path(dip_dir))
			};
		}
		ctx->log->info(pmr_format(ctx, "Failed to read project settings from '{}'", path.string()));
		return std::nullopt;
	}
	ctx->log->info(pmr_format(ctx, "No project settings file found at '{}'", path.string()));
	return std::nullopt;
}

[[nodiscard]]
auto read_registry_yml(context* ctx, const dip::state& state, const std::filesystem::path& path) -> yml_registry {
	if (std::filesystem::exists(path)) {
		ctx->log->info(pmr_format(ctx, "Reading registry from '{}'", path.string()));
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = fkyaml::node::deserialize(*text);
			return yml_registry{};
		}
		ctx->log->info(pmr_format(ctx, "Failed to read registry from '{}'", path.string()));
		return {};
	}
	ctx->log->info(pmr_format(ctx, "No registry file found at '{}'", path.string()));
	return {};
}

[[nodiscard]]
auto read_registry_yml(context* ctx, const dip::state& state, const git_repo_url& path) -> yml_registry {
	throw std::runtime_error(std::format("Reading registry from git repo '{}' is not yet implemented.", path.v));
}

[[nodiscard]]
auto read_registry_yml(context* ctx, const dip::state& state, const yml_project_settings_registry& v) -> yml_registry {
	return std::visit([ctx, &state](const auto& v) { return read_registry_yml(ctx, state, v); }, v);
}

[[nodiscard]]
auto make_default_project_settings_yml(const std::filesystem::path& dip_dir) -> yml_project_settings {
	return yml_project_settings{
		.registry = make_default_registry_yml_file_path(dip_dir)
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

[[nodiscard]]
auto to_string(context*, const git_repo_url& v) -> std::pmr::string {
	return v.v;
}

[[nodiscard]]
auto to_string(context* ctx, const yml_project_settings_registry& registry) -> std::pmr::string {
	return std::visit([ctx](const auto& v) { return to_string(ctx, v); }, registry);
}

auto print_initial_state(context* ctx, const state& state) -> void {
	ctx->log->info(pmr_format(ctx,
		"Using registry: '{}'",
		to_string(ctx, state.project_settings.registry)
	));
}

auto init_state(context* ctx, dip::state* state, const dip::args& args, const requirements& reqs) -> void {
	const auto sys_cache_dir = os::get_system_cache_dir();
	auto cache = args.cache.v.value_or(sys_cache_dir / "dip-cache");
	auto root  = args.root.v.value_or(sys_cache_dir / "dip-root");
	print_info_about_default_directories(ctx, args, sys_cache_dir, cache, root);
	state->dirs.cache                      = cache;
	state->dirs.root                       = root;
	state->dirs.project                    = args.project_dir.v;
	state->dirs.dip                        = find_dip_dir(ctx, state->dirs.project);
	state->file_paths.project_settings_yml = state->dirs.dip / FILENAME_SETTINGS_YML;
	state->project_settings                = read_project_settings_yml(ctx, state->dirs.dip, state->file_paths.project_settings_yml).value_or(make_default_project_settings_yml(state->dirs.dip));
	state->track                           = args.track.v;
	state->install_self                    = args.install_self.v;
	state->verbose                         = args.verbose.v;
	state->prog_paths.git                  = reqs.git_path;
	state->prog_paths.wget                 = reqs.wget_path;
	print_initial_state(ctx, *state);
}

auto read_registry(context* ctx, dip::state* state) -> void {
	state->registry = read_registry_yml(ctx, *state, state->project_settings.registry);
}

[[nodiscard]]
auto make_print_options(const dip::args& args) -> print_options {
	return print_options{
		.errors   = !args.stfu.v,
		.warnings = !(args.quiet.v || args.stfu.v),
		.info     = !(args.quiet.v || args.stfu.v)
	};
}

auto test_process(context* ctx, const std::filesystem::path& prog_path) {
	auto read_stdout = [ctx](const char* bytes, size_t n) {
		ctx->log->info(pmr_format(ctx, "output from stdout: '{}'", std::string_view{bytes, n}));
	};
	auto read_stderr = [ctx](const char* bytes, size_t n) {
		ctx->log->info(pmr_format(ctx, "output from stderr: '{}'", std::string_view{bytes, n}));
	};
	auto proc = TinyProcessLib::Process{prog_path.string(), "", std::move(read_stdout), std::move(read_stderr)};
	auto exit_status = proc.get_exit_status();
	ctx->log->info(pmr_format(ctx, "proc returned with exit status {}", exit_status));
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (const auto reqs = check_requirements(ctx)) {
		auto state = dip::state{};
		init_state(ctx, &state, args, *reqs);
		read_registry(ctx, &state);
		// test_process(ctx, state.prog_paths.git);
		// test_process(ctx, state.prog_paths.wget);
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
