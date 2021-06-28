#!/bin/bash

[ ! -d ./msglib-cpp-git ] && { echo "ERROR: repo not cloned!"; exit 1; }

# Change to the base directory of the repo
cd msglib-cpp-git

# Create build directory and switch to it
mkdir -p build
cd build

# Configure via CMake
cmake -DCMAKE_BUILD_TYPE=DEBUG -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=ON ..

# Build
make