#!/bin/bash
# build with only clobber log pass
set -e

ln -s $(realpath ./passes) llvm/lib/Transforms/Passes

mkdir tmp
cp -rf passes/* tmp/

rm passes/ClobberFunc.cpp
rm passes/GlobalVal.cpp

sed -i -e 's:MemoryIdempotenceAnalysis.cpp;NaiveHook.cpp;ClobberFunc.cpp;GlobalVal.cpp;:MemoryIdempotenceAnalysis.cpp;NaiveHook.cpp;:g' passes/CMakeLists.txt

cd llvm
./build.sh
cd ..
cp llvm/build/lib/RollablePasses.so ClobberPass.so

cp -rf tmp/* passes/
rm -rf tmp/

cd llvm
./build.sh
exit 0
