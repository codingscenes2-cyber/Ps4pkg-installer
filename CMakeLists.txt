cmake_minimum_required(VERSION 3.20)
project(PKGMasterCore VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pkgmaster_core
    src/core/job.cpp
    src/core/queue.cpp
    src/core/url_policy.cpp
    src/platform/installer_bridge.cpp
)
target_include_directories(pkgmaster_core PUBLIC include)
if(MSVC)
  target_compile_options(pkgmaster_core PRIVATE /W4)
else()
  target_compile_options(pkgmaster_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(PKGMASTER_BUILD_TESTS "Build host-side tests" ON)
if(PKGMASTER_BUILD_TESTS)
  enable_testing()
  add_executable(pkgmaster_tests tests/core_tests.cpp)
  target_link_libraries(pkgmaster_tests PRIVATE pkgmaster_core)
  add_test(NAME pkgmaster_core_tests COMMAND pkgmaster_tests)
endif()

# This is a portable-core build only. The PS4 executable/SELF/PKG packaging
# must follow the selected OpenOrbis sample/toolchain, not this host CMake target.
