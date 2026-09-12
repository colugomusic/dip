#pragma once

#include "progs.hpp"
#include "proc.hpp"
#include "string-util.hpp"

namespace dip {

[[nodiscard]]
auto get_latest_git_commit_hash(context* ctx, const dip::prog_paths& progs, std::string_view url, std::string_view branch) -> std::pmr::string {
	const auto args     = pmr_format(ctx, "ls-remote {} {}", url, branch);
	const auto prog_out = run_process_and_return_stdout(ctx, progs.git, args);
	if (prog_out.empty()) {
		throw std::runtime_error{std::format("Failed to get latest commit hash for git repo at '{}' on branch '{}'. Does this branch definitely exist?", url, branch)};
	}
	return get_first_word(ctx, prog_out);
}

[[nodiscard]] auto fn_proc_git(context* ctx, std::filesystem::path git)        { return [ctx, git](std::string_view args) { return run_process_and_return_exit_status(ctx, git, args) == 0; }; }
[[nodiscard]] auto fn_proc_git_stdout(context* ctx, std::filesystem::path git) { return [ctx, git](std::string_view args) { return run_process_and_return_stdout(ctx, git, args); }; }

auto git_checkout(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo, std::string_view revision) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} checkout {}", repo.string(), revision);
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to checkout git repo at '{}' to revision '{}'", repo.string(), revision)};
	}
}

auto git_clone(context* ctx, const std::filesystem::path& git, std::string_view url, const std::filesystem::path& dest) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "clone {} {}", url, dest.string());
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to clone git repo from '{}'", url)};
	}
}

auto git_submodule_update_init_recursive(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} submodule update --init --recursive", repo.string());
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to update/init submodules for git repo at '{}'", repo.string())};
	}
}

auto git_reset_hard(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} reset --hard", repo.string());
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to reset git repo at '{}'", repo.string())};
	}
}

auto git_reset_hard_origin_branch(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo, std::string_view branch) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} reset --hard origin/{}", repo.string(), branch);
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to reset git repo at '{}' to origin/{}", repo.string(), branch)};
	}
}

auto git_clean(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} clean -fd", repo.string());
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to clean git repo at '{}'", repo.string())};
	}
}

auto git_fetch_origin(context* ctx, const std::filesystem::path& git, const std::filesystem::path& repo) -> void {
	const auto fn_git = fn_proc_git(ctx, git);
	const auto args   = pmr_format(ctx, "-C {} fetch origin", repo.string());
	if (!fn_git(args)) {
		throw std::runtime_error{std::format("Failed to fetch origin for git repo at '{}'", repo.string())};
	}
}

[[nodiscard]]
auto git_get_current_branch(context* ctx, const dip::prog_paths& progs, const std::filesystem::path& dir) -> std::pmr::string {
	const auto fn_git = fn_proc_git_stdout(ctx, progs.git);
	const auto args   = pmr_format(ctx, "-C {} symbolic-ref --quiet --short HEAD", dir.string());
	return fn_git(args);
}

[[nodiscard]]
auto reset_existing_git_repo_to_commit(context* ctx, const dip::prog_paths& progs, std::string_view commit, const std::filesystem::path& dir) -> bool {
	try {
		ctx->log->detail(pmr_format(ctx, "Resetting existing git repo at '{}'", dir.string()));
		git_reset_hard(ctx, progs.git, dir);
		git_clean(ctx, progs.git, dir);
		git_fetch_origin(ctx, progs.git, dir);
		git_checkout(ctx, progs.git, dir, commit);
		git_reset_hard(ctx, progs.git, dir);
		git_submodule_update_init_recursive(ctx, progs.git, dir);
		return true;
	}
	catch (const std::exception& e) {
		ctx->log->detail(pmr_format(ctx, "Failed to reset existing git repo at '{}'. Exception: {}", dir.string(), e.what()));
		return false;
	}
	catch (...) {
		ctx->log->detail(pmr_format(ctx, "Failed to reset existing git repo at '{}'.", dir.string()));
		return false;
	}
}

auto nuke_and_reclone(context* ctx, const dip::prog_paths& progs, std::string_view url, std::string_view revision, const std::filesystem::path& dest) -> void {
	if (std::filesystem::exists(dest)) {
		ctx->log->detail(pmr_format(ctx, "Removing existing directory at '{}'", dest.string()));
		std::filesystem::remove_all(dest);
	}
	ctx->log->detail(pmr_format(ctx, "Cloning git repo from '{}' to '{}'", url, dest.string()));
	git_clone(ctx, progs.git, url, dest);
	if (!revision.empty()) {
		git_checkout(ctx, progs.git, dest, revision);
	}
	git_submodule_update_init_recursive(ctx, progs.git, dest);
}

auto git_clone(context* ctx, const dip::prog_paths& progs, std::string_view url, std::string_view commit, const std::filesystem::path& dest) -> void {
	if (std::filesystem::exists(dest / ".git")) {
		if (reset_existing_git_repo_to_commit(ctx, progs, commit, dest)) {
			return;
		}
	}
	// Either isn't an existing repo or we failed to reset it for some reason.
	// Just nuke it and re-clone.
	nuke_and_reclone(ctx, progs, url, commit, dest);
}

} // dip
