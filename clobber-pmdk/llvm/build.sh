#!/bin/bash
set -e

mkdir -p build
cmake -B build -Wno-dev -DCMAKE_BUILD_TYPE=RELEASE -DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-alpine-linux-musl \
	-DLLVM_TARGETS_TO_BUILD='X86' -DLLVM_ENABLE_DUMP=ON -G Ninja .
# atlas needs ENABLE_DUMP on the release build
cd build
ninja
cd ..
exit 0

