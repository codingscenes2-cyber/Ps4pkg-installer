#!/usr/bin/env sh
set -eu

BUILD_DIR="${1:-build}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DPKGMASTER_BUILD_TESTS=ON \
  -DPKGMASTER_BUILD_HOST_SERVICE=ON
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
