#!/bin/bash
if [ ! -d build_linux ]; then
    mkdir build_linux
fi

build_type=Release

cd build_linux
if [ $# -gt 0 ] && [ $1 = "debug" ]; then
    build_type=Debug
fi
echo "$build_type"

cmake $* -DCMAKE_BUILD_TYPE=$build_type ../ 
make -j2 

