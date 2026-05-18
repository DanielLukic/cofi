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
pkg_config_packages="gtk+-3.0 x11 gio-2.0 json-glib-1.0"
export CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -g $(pkg-config --cflags $pkg_config_packages)"
export LDFLAGS="$(pkg-config --libs $pkg_config_packages) -lm"

make BUILD_NUMBER="${BUILD_NUMBER:-0}"

echo "Build completed successfully!"
