#!/bin/bash
set -e

LLVM_PATH="$(realpath ../llvm/build/bin)"
export PATH="${LLVM_PATH}:$PATH"

#build compiler
cd compiler-plugin
if [ ! -d "build-all" ]; then
	set +e
    ./build_plugin
	if [[ $? -ne 0 ]]; then
		cat ./build_log.txt
		exit 1
	fi
	set -e
fi

#set wrapper flags
#export CFLAGS="-Wl,-wrap,pthread_mutex_lock"
#export CFLAGS="$CFLAGS -Wl,-wrap,pthread_mutex_trylock"
#export CFLAGS="$CFLAGS -Wl,-wrap,pthread_mutex_unlock"
#export CFLAGS="$CFLAGS -Wl,-wrap,pthread_create"
#export CFLAGS="$CFLAGS -Wno-error=unused-command-line-argument"

#build runtime
cd ..
cd runtime
sed -i 's:-O3:-O3 -fPIC -D_DISABLE_HELPER:g' CMakeLists.txt
echo 'add_library (atlas-shared SHARED $<TARGET_OBJECTS:Cache_flush> $<TARGET_OBJECTS:Consistency> $<TARGET_OBJECTS:Logger> $<TARGET_OBJECTS:Util> $<TARGET_OBJECTS:Pregion_mgr> $<TARGET_OBJECTS:Pmalloc>) #defaults to static build' >> CMakeLists.txt
sed -i 's:/dev/shm/:/mnt/pmem/:g' src/util/util.cpp

sed -i 's:kPRegionSize_ = 4:kPRegionSize_ = 18:g' src/internal_includes/pregion_configs.hpp
sed -i 's:kNumArenas_ = 64:kNumArenas_ = 8:g' src/internal_includes/pregion_configs.hpp
#sed -i 's:kHashTableSize = 1 << 10:kHashTableSize = (uint64_t)1 << 16:g' src/internal_includes/log_configs.hpp
sed -i -e '582d;593,594d;596d' src/pregion_mgr/pregion_mgr.cpp

if [ -d "build-all" ];
then
   rm -r build-all
fi

mkdir build-all
cd build-all
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j8
if [ $? -ne 0 ]; then
    echo 'Unable to make Atlas'
    exit 1
fi

exit 0
