#pragma once

#include "const-strings.hpp"
#include "os.hpp"
#include "pmr-format.hpp"
#include "string-util.hpp"

namespace dip {

struct requirements {
	std::filesystem::path cmake_path;
	std::filesystem::path git_path;
	std::filesystem::path wget_path;
	std::filesystem::path zip_path;
};

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
	const auto cmake = check_program_available(ctx, "cmake", &missing_programs);
	const auto git   = check_program_available(ctx, "git", &missing_programs);
	const auto wget  = check_program_available(ctx, "wget", &missing_programs);
	const auto zip   = check_program_available(ctx, "7z", &missing_programs);
	if (!missing_programs.empty()) {
		ctx->log->error(make_missing_programs_error(ctx, missing_programs));
		return std::nullopt;
	}
	return requirements{
		.cmake_path = *cmake,
		.git_path   = *git,
		.wget_path  = *wget,
		.zip_path   = *zip
	};
}

} // dip
