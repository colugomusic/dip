#pragma once

namespace dip {

static constexpr auto PROGRAM_NAME           = "dip";
static constexpr auto PROGRAM_VERSION        = "1.0.0";
static constexpr auto ARG_CACHE_NAME_SHORT   = "-c";
static constexpr auto ARG_CACHE_NAME_LONG    = "-c";
static constexpr auto ARG_CACHE_HELP         = "Where to store source files and build dependencies";
static constexpr auto ARG_ROOT_NAME_SHORT    = "-r";
static constexpr auto ARG_ROOT_NAME_LONG     = "--root";
static constexpr auto ARG_ROOT_HELP          = "Where to install dependencies";
static constexpr auto ARG_QUIET_NAME_SHORT   = "-q";
static constexpr auto ARG_QUIET_NAME_LONG    = "--quiet";
static constexpr auto ARG_QUIET_HELP         = "Only print errors.";
static constexpr auto ARG_STFU_NAME_LONG     = "--stfu";
static constexpr auto ARG_STFU_HELP          = "Don't print anything.";
static constexpr auto ARG_PROJECT_NAME_SHORT = "-p";
static constexpr auto ARG_PROJECT_NAME_LONG  = "--project";
static constexpr auto ARG_PROJECT_HELP       = "Path to the project you want to install dependencies for.";
static constexpr auto ARG_TRACK_NAME_SHORT   = "-t";
static constexpr auto ARG_TRACK_NAME_LONG    = "--track";
static constexpr auto ARG_TRACK_ALL_VALUE    = "__dip_arg_track_all__";
static constexpr auto ARG_TRACK_HELP         = "Update tag to latest commit hash for git dependencies. Use `--track` without args to update all git dependencies which specify `track=true`, or specify names or specific dependencies to update separated by commas, e.g. `--track=foo,bar,baz`.";
static constexpr auto ARG_VERBOSE_NAME_SHORT = "-v";
static constexpr auto ARG_VERBOSE_NAME_LONG  = "--verbose";
static constexpr auto ARG_VERBOSE_HELP       = "Verbose output";
static constexpr auto FILENAME_DEPS_YML      = "deps.yml";
static constexpr auto FILENAME_REGISTRY_YML  = "registry.yml";
static constexpr auto FILENAME_SETTINGS_YML  = "settings.yml";
static constexpr auto DIR_PROJECT_DIP        = ".dip";
static constexpr auto DIR_PROJECT_DOPE       = "dope";
static constexpr auto KEY_GIT                = "git";
static constexpr auto KEY_REGISTRY           = "registry";

} // dip
