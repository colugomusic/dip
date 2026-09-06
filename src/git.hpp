#pragma once

#include "progs.hpp"
#include "proc.hpp"
#include "string-util.hpp"

namespace dip {

[[nodiscard]]
auto get_latest_git_commit_hash(context* ctx, const dip::prog_paths& progs, std::string_view url) -> std::pmr::string {
	const auto args     = pmr_format(ctx, "ls-remote {} HEAD", url);
	const auto prog_out = run_process_and_return_stdout(ctx, progs.git, args);
	return get_first_word(ctx, prog_out);
}

} // dip
