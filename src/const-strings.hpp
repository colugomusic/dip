#pragma once

namespace dip {

static constexpr auto ARG_CACHE_CLEAN_HELP     = "Clean up unreferenced sources and build files. When --cache-clean is specified\n"
                                                 "then the --root argument can be a comma-separated list of absolute paths to to\n"
                                                 "roots for which the installed dependencies should be preserved. Otherwise\n"
                                                 "every install prefix in the default root folder will be preserved.";
static constexpr auto ARG_CACHE_CLEAN_LONG     = "--cache-clean";
static constexpr auto ARG_CACHE_HELP           = "Where to store source files and build dependencies.";
static constexpr auto ARG_CACHE_LONG           = "--cache";
static constexpr auto ARG_CACHE_SHORT          = "-c";
static constexpr auto ARG_CFG_HELP             = "Specify the cmake configs to install, separated by commas,\n"
                                                 "e.g. `--cfg Debug,RelWithDebInfo`.\n"
                                                 "If not specified then the list specified by the `cmake-configs` mapping in\n"
                                                 "settings.yml are used.";
static constexpr auto ARG_CFG_LONG             = "--cfg";
static constexpr auto ARG_FULL_PKG_CHECK_HELP  = "Try CMake's find_package() on every dependency to check that it's properly\n"
                                                 "installed. This check always happens regardless of this argument immediately\n"
                                                 "after a dependency is installed, but this flag forces dip to re-check every\n"
                                                 "dependency.";
static constexpr auto ARG_FULL_PKG_CHECK_LONG  = "--check";
static constexpr auto ARG_INSTALL_SELF_HELP    = "Install this project (in addition to its dependencies).";
static constexpr auto ARG_INSTALL_SELF_LONG    = "--self";
static constexpr auto ARG_INSTALL_SELF_SHORT   = "-s";
static constexpr auto ARG_PROJECT_HELP         = "Path to the project you want to install dependencies for. If not specified,\n"
                                                 "the current working directory is assumed to be the project.";
static constexpr auto ARG_PROJECT_LONG         = "--project";
static constexpr auto ARG_PROJECT_SHORT        = "-p";
static constexpr auto ARG_QUIET_HELP           = "Only print errors.";
static constexpr auto ARG_QUIET_LONG           = "--quiet";
static constexpr auto ARG_QUIET_SHORT          = "-q";
static constexpr auto ARG_REACQUIRE_HELP       = "Reacquire dependencies from their origins. Specify names of specific\n"
                                                 "dependencies to reacquire separated by commas,\n"
                                                 "e.g. `--reacquire foo,bar,baz`.";
static constexpr auto ARG_REACQUIRE_LONG       = "--reacquire";
static constexpr auto ARG_REINSTALL_HELP       = "Reinstall dependencies. Specify names of specific dependencies to reinstall\n"
                                                 "separated by commas,\n"
                                                 "e.g. `--reinstall foo,bar,baz`.";
static constexpr auto ARG_REINSTALL_LONG       = "--reinstall";
static constexpr auto ARG_ROOT_HELP            = "Where to install dependencies.";
static constexpr auto ARG_ROOT_LONG            = "--root";
static constexpr auto ARG_ROOT_SHORT           = "-r";
static constexpr auto ARG_STFU_HELP            = "Don't print anything.";
static constexpr auto ARG_STFU_LONG            = "--stfu";
static constexpr auto ARG_TRACK_ALL_VALUE      = "__dip_arg_track_all__";
static constexpr auto ARG_TRACK_HELP           = "Update tag to latest commit hash for git dependencies. Use `--track` without\n"
                                                 "args to update all git dependencies which specify `track: <branch name>`, or\n"
                                                 "specify names of specific dependencies to update separated by commas,\n"
                                                 "e.g. `--track foo,bar,baz`.";
static constexpr auto ARG_TRACK_LONG           = "--track";
static constexpr auto ARG_TRACK_SHORT          = "-t";
static constexpr auto ARG_VERBOSE_HELP         = "Verbose output";
static constexpr auto ARG_VERBOSE_LONG         = "--verbose";
static constexpr auto ARG_VERBOSE_SHORT        = "-v";
static constexpr auto DIR_PROJECT_DIP          = ".dip";
static constexpr auto FILENAME_REGISTRY_YML    = "registry.yml";
static constexpr auto FILENAME_SETTINGS_YML    = "settings.yml";
static constexpr auto KEY_CMAKE_CONFIGS        = "cmake-configs";
static constexpr auto KEY_CMAKE_OPTIONS        = "cmake-options";
static constexpr auto KEY_CMAKE_OPTIONS_LIN    = "cmake-options-lin";
static constexpr auto KEY_CMAKE_OPTIONS_MAC    = "cmake-options-mac";
static constexpr auto KEY_CMAKE_OPTIONS_WIN    = "cmake-options-win";
static constexpr auto KEY_COMMIT               = "commit";
static constexpr auto KEY_GIT                  = "git";
static constexpr auto KEY_PACKAGE_NAMES        = "package-names";
static constexpr auto KEY_MD5                  = "md5";
static constexpr auto KEY_NAME                 = "name";
static constexpr auto KEY_PATH                 = "path";
static constexpr auto KEY_TRACK                = "track";
static constexpr auto KEY_URL                  = "url";
static constexpr auto KEY_VERSION              = "version";
static constexpr auto PROGRAM_NAME             = "dip";
static constexpr auto PROGRAM_VERSION          = "1.0.0";

} // dip
