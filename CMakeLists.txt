cmake_minimum_required(VERSION 4.1.0)
project(dip)
set(is_macos   "$<PLATFORM_ID:Darwin>")
set(is_linux   "$<PLATFORM_ID:Linux>")
set(is_windows "$<PLATFORM_ID:Windows>")
set(is_unix    "$<OR:${is_linux},${is_macos}>")
add_library(argparse INTERFACE)
add_library(fkyaml INTERFACE)
add_library(rang INTERFACE)
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
target_sources(rang INTERFACE
	FILE_SET  HEADERS
	BASE_DIRS src/extern
	FILES     src/extern/rang.hpp
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
	src/cmake.hpp
	src/const-strings.hpp
	src/context.hpp
	src/dep.hpp
	src/dirs.hpp
	src/fs.hpp
	src/git.hpp
	src/list-util.hpp
	src/main.cpp
	src/os.hpp
	src/pmr-format.hpp
	src/proc.hpp
	src/progs.hpp
	src/requirements.hpp
	src/string-util.hpp
	src/yaml.hpp
	$<${is_linux}:src/os-lin.cpp>
	$<${is_macos}:src/os-mac.cpp>
	$<${is_windows}:src/os-win.cpp>
)
target_link_libraries(dip PRIVATE
	argparse
	fkyaml
	rang
	tiny-process-library
)
