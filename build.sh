#!/bin/sh

BUILD_TYPE=release
PRESET=x64-linux-$BUILD_TYPE
BUILD_DIR=./out/build/$PRESET

cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE
cmake --build $BUILD_DIR --config $BUILD_TYPE
