cmake_minimum_required(VERSION 3.16)
project(cpm8000_emu LANGUAGES CXX)

# Cross-platform build for the hosted CP/M-8000 emulator (the `cpm8k` host
# program). The CP/M-8000 toolchain, BIOS assembly, and system generation are
# driven by the top-level Makefile (they run the guest DR/Zilog tools inside the
# emulator); this CMake build produces only the C++ emulator itself.
#
#   cmake -S . -B build/emu -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/emu                 # -> build/emu/cpm8k
#
# `make emu` drives this under the hood. Portable across Linux and macOS; the
# console/file layers use POSIX (termios/unistd/select), so a Windows port would
# need those abstracted first.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Z8001 CPU emulator library only (skip its driver/tools/tests).
set(BUILD_DRIVER OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS  OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS  OFF CACHE BOOL "" FORCE)
add_subdirectory(z8000_emu)

add_executable(cpm8k
  src/cpm8kemu/main.cpp
  src/cpm8kemu/cpm8k_bdos.cpp
  src/cpm8kemu/cpm8k_bios.cpp
  src/cpm8kemu/cpm8k_console.cpp
  src/cpm8kemu/cpm8k_file.cpp
  src/cpm8kemu/cpm8k_drives.cpp
)

# Linking z8000 brings its PUBLIC include dir (lib/include) so <z8000/...>
# resolves; add lib/include/z8000 so the emulator's `#include "z8000.h"` does too.
target_include_directories(cpm8k PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/z8000_emu/lib/include/z8000
)
target_link_libraries(cpm8k PRIVATE z8000)

if(MSVC)
  target_compile_options(cpm8k PRIVATE /W3)
else()
  target_compile_options(cpm8k PRIVATE -Wall -Wextra)
endif()
