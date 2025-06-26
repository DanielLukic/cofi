#!/bin/bash
set -e

# Build script for COFI
# This ensures consistent builds across environments

echo "=== COFI Build Script ==="
echo "Build environment:"
echo "  GCC version: $(gcc --version | head -n1)"
echo "  GTK version: $(pkg-config --modversion gtk+-3.0 2>/dev/null || echo 'not found')"
echo "  Build number: ${BUILD_NUMBER:-0}"
echo ""

# Clean previous builds
echo "Cleaning previous build..."
make clean

# Build with consistent flags
echo "Building COFI..."
export CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -g $(pkg-config --cflags gtk+-3.0 x11)"
export LDFLAGS="$(pkg-config --libs gtk+-3.0 x11) -lm"

make BUILD_NUMBER="${BUILD_NUMBER:-0}"

echo "Build completed successfully!"