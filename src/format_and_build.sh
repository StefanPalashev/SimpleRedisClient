#!/bin/bash

BUILD_TYPE=${1:-debug}
OUTPUT_DIR="../build/Debug"
BUILD_TARGET="DEBUG"

if [ "$BUILD_TYPE" = "debug" ]; then
    CXXFLAGS="-g -fstandalone-debug -fdiagnostics-color=always"
else
    CXXFLAGS="-O3 -DNDEBUG -fdiagnostics-color=always"
    OUTPUT_DIR="../build/Release"
    BUILD_TARGET="RELEASE"
fi

mkdir -p $OUTPUT_DIR

echo Formatting...
find . -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' -exec clang-format -style=file -i {} \;
find ../include -regex '.*\.\(cpp\|hpp\|cu\|c\|h\)' -exec clang-format -style=file -i {} \;

echo Build type [$BUILD_TARGET], C++ flags: $CXXFLAGS

# Enable globstar for recursive globbing
shopt -s globstar

echo Building...
/usr/bin/clang++-11 \
    -std=c++17 \
    $CXXFLAGS \
    -L/usr/local/lib -lhiredis \
     **/*.cpp \
    -o $OUTPUT_DIR/simple_redis_client

# Check if the build command was successful
if [ $? -eq 0 ]; then
    echo "Done! Executable located in $OUTPUT_DIR/simple_redis_client"
else
    echo "Build failed."
fi