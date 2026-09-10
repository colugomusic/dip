#pragma once

namespace dip {

static constexpr auto PROGRAM_NAME            = "dip";
static constexpr auto PROGRAM_VERSION         = "1.0.0";
static constexpr auto ARG_CFG_NAME_LONG       = "--cfg";
static constexpr auto ARG_CFG_HELP            = "Specify the cmake configs to install, separated by commas, e.g. `--cfg Debug,RelWithDebInfo`. If not specified then the list specified by the `cmake-configs` mapping in settings.yml are used.";
static constexpr auto ARG_CACHE_NAME_SHORT    = "-c";
static constexpr auto ARG_CACHE_NAME_LONG     = "--cache";
static constexpr auto ARG_CACHE_HELP          = "Where to store source files and build dependencies";
static constexpr auto ARG_ROOT_NAME_SHORT     = "-r";
static constexpr auto ARG_ROOT_NAME_LONG      = "--root";
static constexpr auto ARG_ROOT_HELP           = "Where to install dependencies";
static constexpr auto ARG_QUIET_NAME_SHORT    = "-q";
static constexpr auto ARG_QUIET_NAME_LONG     = "--quiet";
static constexpr auto ARG_QUIET_HELP          = "Only print errors.";
static constexpr auto ARG_STFU_NAME_LONG      = "--stfu";
static constexpr auto ARG_STFU_HELP           = "Don't print anything.";
static constexpr auto ARG_PROJECT_NAME_SHORT  = "-p";
static constexpr auto ARG_PROJECT_NAME_LONG   = "--project";
static constexpr auto ARG_PROJECT_HELP        = "Path to the project you want to install dependencies for. If not specified, the current working directory is assumed to be the project.";
static constexpr auto ARG_REACQUIRE_NAME_LONG = "--reacquire";
static constexpr auto ARG_REACQUIRE_HELP      = "Reacquire dependencies from their origins. Specify names of specific dependencies to reacquire separated by commas, e.g. `--reacquire foo,bar,baz`.";
static constexpr auto ARG_TRACK_NAME_SHORT    = "-t";
static constexpr auto ARG_TRACK_NAME_LONG     = "--track";
static constexpr auto ARG_TRACK_ALL_VALUE     = "__dip_arg_track_all__";
static constexpr auto ARG_TRACK_HELP          = "Update tag to latest commit hash for git dependencies. Use `--track` without args to update all git dependencies which specify `track: <branch name>`, or specify names of specific dependencies to update separated by commas, e.g. `--track foo,bar,baz`.";
static constexpr auto ARG_VERBOSE_NAME_SHORT  = "-v";
static constexpr auto ARG_VERBOSE_NAME_LONG   = "--verbose";
static constexpr auto ARG_VERBOSE_HELP        = "Verbose output";
static constexpr auto FILENAME_REGISTRY_YML   = "registry.yml";
static constexpr auto FILENAME_SETTINGS_YML   = "settings.yml";
static constexpr auto DIR_PROJECT_DIP         = ".dip";
static constexpr auto KEY_CMAKE_CONFIGS       = "cmake-configs";
static constexpr auto KEY_GIT                 = "git";
static constexpr auto KEY_PATH                = "path";
static constexpr auto KEY_NAME                = "name";
static constexpr auto KEY_URL                 = "url";
static constexpr auto KEY_CMAKE_OPTIONS       = "cmake-options";
static constexpr auto KEY_CMAKE_OPTIONS_MAC   = "cmake-options-mac";
static constexpr auto KEY_CMAKE_OPTIONS_LIN   = "cmake-options-lin";
static constexpr auto KEY_CMAKE_OPTIONS_WIN   = "cmake-options-win";
static constexpr auto KEY_COMMIT              = "commit";
static constexpr auto KEY_TRACK               = "track";
static constexpr auto KEY_MD5                 = "md5";

} // dip
