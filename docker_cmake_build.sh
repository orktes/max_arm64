#!/bin/bash

set -e

echo "=== Docker CMake Build ==="

BUILD_TYPE="${BUILD_TYPE:-default}"

echo "Build type: $BUILD_TYPE"

# Install CMake and build tools if not available
if ! command -v cmake &> /dev/null; then
    echo "Installing CMake and build tools..."
    apt-get update
    apt-get install -y cmake build-essential
fi

# Clean any existing build directories to avoid conflicts
echo "Cleaning build directories..."
rm -rf build build_x11 build_debug build_x11_debug build_docker

# Configure CMake for ARM64 device build (use separate build dir for Docker)
echo "Configuring with CMake..."
cmake -B build_docker \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_X11_DESKTOP=OFF \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -G "Unix Makefiles"

echo "Building..."
cmake --build build_docker -j$(nproc)

# Copy the built binary to the expected location
echo "Copying binary..."
cp build_docker/maxpayne_arm64 ./maxpayne_arm64

echo "Docker build complete! Binary: maxpayne_arm64"