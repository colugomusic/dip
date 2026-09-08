cmake_minimum_required(VERSION 4.1.0)
project(dip)
set(is_macos   "$<PLATFORM_ID:Darwin>")
set(is_linux   "$<PLATFORM_ID:Linux>")
set(is_windows "$<PLATFORM_ID:Windows>")
set(is_unix    "$<OR:${is_linux},${is_macos}>")
add_library(argparse INTERFACE)
add_library(fkyaml INTERFACE)
add_library(hash-library STATIC)
add_library(tiny-process-library STATIC)
add_executable(dip)
target_sources(argparse INTERFACE
	FILE_SET  HEADERS
	BASE_DIRS src/extern
	FILES     src/extern/argparse.hpp
)
target_sources(fkyaml INTERFACE
	FILE_SET  HEADERS
	BASE_DIRS src/extern
	FILES     src/extern/fkYAML/fkyaml_fwd.hpp
	          src/extern/fkYAML/node.hpp
)
target_sources(hash-library PRIVATE
	src/extern/hash-library-md5.cpp
)
target_sources(hash-library INTERFACE
	FILE_SET  HEADERS
	BASE_DIRS src/extern
	FILES     src/extern/hash-library-md5.h
)
target_sources(tiny-process-library PRIVATE
	src/extern/tiny-process-library/process.cpp
	$<${is_unix}:src/extern/tiny-process-library/process_unix.cpp>
	$<${is_windows}:src/extern/tiny-process-library/process_win.cpp>
)
target_sources(tiny-process-library INTERFACE
	FILE_SET  HEADERS
	BASE_DIRS src/extern/tiny-process-library
	FILES     src/extern/tiny-process-library/process.hpp
)
target_sources(dip PRIVATE
	src/args.hpp
	src/cfg.hpp
	src/cmake.hpp
	src/cmake-options.hpp
	src/const-strings.hpp
	src/context.hpp
	src/dep.hpp
	src/dirs.hpp
	src/fs.hpp
	src/git.hpp
	src/list-util.hpp
	src/main.cpp
	src/md5.hpp
	src/os.hpp
	src/pmr-format.hpp
	src/proc.hpp
	src/progs.hpp
	src/requirements.hpp
	src/string-util.hpp
	src/version.hpp
	src/yaml.hpp
	src/wget.hpp
	$<${is_linux}:src/os-lin.cpp>
	$<${is_macos}:src/os-mac.cpp>
	$<${is_windows}:src/os-win.cpp>
)
target_link_libraries(dip PRIVATE
	argparse
	fkyaml
	hash-library
	tiny-process-library
)
