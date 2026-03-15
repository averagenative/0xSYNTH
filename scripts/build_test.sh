#!/bin/bash
#
# Quick build + test cycle. Run this instead of typing cmake/make/ctest manually.
# Usage: ./scripts/build_test.sh [test-name]
#
# Examples:
#   ./scripts/build_test.sh           # build + run all tests
#   ./scripts/build_test.sh fm        # build + run only FM tests
#   ./scripts/build_test.sh foundation # build + run only foundation tests

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure if needed
if [ ! -f Makefile ]; then
    cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Debug
fi

# Build
make -j$(nproc) 2>&1 | grep -E "error|warning|Built|Linking" || true

# Test
if [ -n "$1" ]; then
    ctest --output-on-failure -R "$1"
else
    ctest --output-on-failure
fi
