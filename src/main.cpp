#include "args.hpp"
#include "colors.hpp"
#include "os.hpp"
#include <fkYAML/node.hpp>
#include <fstream>
#include <rang.hpp>
#include <tiny-process-library/process.hpp>
#include <span>
#include <string>
#include <vector>

namespace dip {

auto operator""_MB(uint64_t v) -> uint64_t { return 1024 * 1024 * v; }

struct requirements {
	std::filesystem::path git_path;
	std::filesystem::path wget_path;
};

struct git_repo_url {
	std::pmr::string v;
};

using yml_project_settings_registry = std::variant<std::monostate, std::filesystem::path, git_repo_url>;

struct yml_project_settings {
	yml_project_settings_registry registry;
};

struct yml_registry {
};

struct dirs {
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

template <typename... Args> [[nodiscard]]
auto pmr_format(const context* ctx, std::format_string<Args...> fmt, Args&&... args) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	std::format_to(std::back_inserter(str), fmt, std::forward<decltype(args)>(args)...);
	return str;
}

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
auto get_root(context* ctx, const dip::args& args) -> std::filesystem::path {
	if (args.root.v) {
		return *args.root.v;
	}
	ctx->log->info("No root specified.");
	const auto sys_cache_dir = os::get_system_cache_dir();
	ctx->log->info(pmr_format(ctx, "System cache folder is: '{}'", sys_cache_dir.string()));
	const auto root = sys_cache_dir / "dip";
	ctx->log->info(pmr_format(ctx,
		"Using '{}' as root because you didn't specify one.\n"
		"If you're not happy with this then specify a root with --root \"path/to/root\"", root.string()));
	return root;
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
			if (node.contains(KEY_GIT)) {
				const auto git_node = value_node.at(KEY_GIT);
				if (git_node.is_string()) {
					const auto str = git_node.get_value<std::string>();
					return git_repo_url{std::pmr::string{str.data(), str.size(), ctx->mem}};
				}
			}
		}
	}
	else {
		ctx->log->info(pmr_format(ctx,
			"No '{}' key was found in '{}'.\n"
			"I'm going to assume there's a registry at '{}'.",
			KEY_REGISTRY,
			settings_yml_file_path.string(),
			default_registry_yml_file_path.string()));
		return default_registry_yml_file_path;
	}
	return {};
}

[[nodiscard]]
auto make_default_registry_yml_file_path(context* ctx, const std::filesystem::path& dip_dir) -> std::filesystem::path {
	return dip_dir / FILENAME_REGISTRY_YML;
}

[[nodiscard]]
auto read_project_settings_yml(context* ctx, const std::filesystem::path& dip_dir, const std::filesystem::path& path) -> yml_project_settings {
	auto yml = yml_project_settings{};
	ctx->log->info(pmr_format(ctx, "Reading project settings from '{}'", path.string()));
	if (std::filesystem::exists(path)) {
		if (const auto text = read_file_text(ctx, path)) {
			const auto node = fkyaml::node::deserialize(*text);
			yml.registry    = read_registry(ctx, node, path, make_default_registry_yml_file_path(ctx, dip_dir));
		}
	}
	return {};
}

[[nodiscard]]
auto init_state(context* ctx, const dip::args& args, const std::filesystem::path& root, const requirements& reqs) -> state {
	auto state                            = dip::state{};
	state.dirs.root                       = root;
	state.dirs.project                    = args.project_dir.v;
	state.dirs.dip                        = find_dip_dir(ctx, state.dirs.project);
	state.file_paths.project_settings_yml = state.dirs.dip / FILENAME_SETTINGS_YML;
	state.project_settings                = read_project_settings_yml(ctx, state.dirs.dip, state.file_paths.project_settings_yml);
	state.track                           = args.track.v;
	state.install_self                    = args.install_self.v;
	state.verbose                         = args.verbose.v;
	state.prog_paths.git                  = reqs.git_path;
	state.prog_paths.wget                 = reqs.wget_path;
	return state;
}

[[nodiscard]]
auto to_string(context*, const std::pmr::string& v) -> std::pmr::string {
	return v;
}

[[nodiscard]]
auto to_string(context* ctx, const std::filesystem::path& v) -> std::pmr::string {
	return v.string<char, std::char_traits<char>, std::pmr::polymorphic_allocator<char>>(ctx->mem);
}

template <typename T> [[nodiscard]]
auto join(context* ctx, std::span<const T> items, std::string_view delimiter) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	for (size_t i = 0; i < items.size(); ++i) {
		str += to_string(ctx, items[i]);
		if (i < items.size() - 1) {
			str += delimiter;
		}
	}
	return str;
}

[[nodiscard]]
auto check_program_available(context* ctx, std::string_view name, std::pmr::vector<std::pmr::string>* missing_list) -> std::optional<std::filesystem::path> {
	const auto program_filename = os::get_program_filename(name);
	const auto name_with_a_space_after_it = pmr_format(ctx, "{} ", name);
	if (const auto path = os::resolve_program_path(program_filename, ctx->env_paths)) {
		ctx->log->info(pmr_format(ctx, " * {:-<10} found at '{}'", name_with_a_space_after_it, path->string()));
		return path;
	}
	else {
		ctx->log->info(pmr_format(ctx, " * {:-<10} not found", name_with_a_space_after_it));
		if (missing_list) {
			missing_list->emplace_back(name);
		}
		return std::nullopt;
	}
}

[[nodiscard]]
auto str_alongside_dip_exe(context* ctx) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	std::format_to(std::back_inserter(str), "Alongside {}", os::get_program_filename(PROGRAM_NAME).string());
	return str;
}

[[nodiscard]]
auto str_the_cwd(context* ctx) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	std::format_to(std::back_inserter(str), "The current working directory ({})", std::filesystem::current_path().string());
	return str;
}

[[nodiscard]]
auto get_places_to_put_programs(context* ctx) -> std::pmr::vector<std::pmr::string> {
	auto places = std::pmr::vector<std::pmr::string>{ctx->mem};
	places.emplace_back(str_the_cwd(ctx));
	places.emplace_back(str_alongside_dip_exe(ctx));
	places.emplace_back("Somewhere in your PATH");
	return places;
}

[[nodiscard]]
auto make_download_help(context* ctx, std::span<const std::pmr::string> programs) -> std::pmr::string {
	auto str = std::pmr::string{ctx->mem};
	for (const auto& program : programs) {
		if (const auto help = os::get_program_download_help(program, ctx->mem); !help.empty()) {
			std::format_to(std::back_inserter(str), "{}\n", help);
		}
	}
	return str;
}

[[nodiscard]]
auto make_missing_programs_error(context* ctx, std::span<const std::pmr::string> programs) -> std::pmr::string {
    const auto places_to_put_programs = get_places_to_put_programs(ctx);
    const auto download_help          = make_download_help(ctx, programs);
	return pmr_format(ctx,
		"Hello! You need to download the following programs:\n"
		" * {}\n\n"
		"and then put them somewhere where I can find them. Possible places to put them:\n\n"
		" * {}\n\n"
		"{}",
		join<std::pmr::string>(ctx, programs, "\n * "),
		join<std::pmr::string>(ctx, places_to_put_programs, "\n * "),
		download_help);
}

[[nodiscard]]
auto check_requirements(context* ctx) -> std::optional<requirements> {
	auto missing_programs = std::pmr::vector<std::pmr::string>{ctx->mem};
	ctx->log->info("Checking requirements...");
	const auto git  = check_program_available(ctx, "git", &missing_programs);
	const auto wget = check_program_available(ctx, "wget", &missing_programs);
	if (!missing_programs.empty()) {
		ctx->log->error(make_missing_programs_error(ctx, missing_programs));
		return std::nullopt;
	}
	return requirements{
		.git_path  = *git,
		.wget_path = *wget
	};
}

[[nodiscard]]
auto make_print_options(const dip::args& args) -> print_options {
	return print_options{
		.errors   = !args.stfu.v,
		.warnings = !(args.quiet.v || args.stfu.v),
		.info     = !(args.quiet.v || args.stfu.v)
	};
}

auto test_process(const std::filesystem::path& prog_path) {
	auto read_stdout = [](const char* bytes, size_t n) {
		std::cout << "output from stdout: " << std::string_view{bytes, n} << "\n";
	};
	auto read_stderr = [](const char* bytes, size_t n) {
		std::cout << "output from stderr: " << std::string_view{bytes, n} << "\n";
	};
	auto proc = TinyProcessLib::Process{prog_path.string(), "", std::move(read_stdout), std::move(read_stderr)};
	auto exit_status = proc.get_exit_status();
	std::cout << "proc returned with exit status " << exit_status << "\n";
}

[[nodiscard]]
auto happy_path(context* ctx, int argc, const char* argv[]) -> int {
	os::enable_ansi_colors();
	const auto args    = get_args(ctx, argc, argv);
	ctx->print_options = make_print_options(args);
	if (const auto reqs = check_requirements(ctx)) {
		const auto root  = get_root(ctx, args);
		const auto state = init_state(ctx, args, root, *reqs);
		//test_process(state.prog_paths.git);
		//test_process(state.prog_paths.wget);
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
