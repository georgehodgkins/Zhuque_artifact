# Build image with Zhuque as the system libc 
# 
FROM alpine:3.14 AS sysbase

ENV UTILS='bash vim tmux gdb strace git mandoc util-linux valgrind perf python3'
ENV DEPS='gcc g++ make ninja musl-dev linux-headers'

RUN apk update && apk add $DEPS $UTILS

FROM sysbase AS builder
ARG libc_flags=''
ENV LIBC_CFLAGS '-O2 -pipe -g -DNDEBUG' $libc_flags
#ENV LIBC_CFLAGS '-O0 -pipe -g' $libc_flags
ADD musl-src /musl-libc
RUN cd musl-libc && CFLAGS=$LIBC_CFLAGS ./configure --prefix=/usr && \
	make -j && src/stdatomic/redefine_syms.sh

FROM builder AS installer
RUN cd musl-libc && make install

FROM installer AS libtest
ADD libc-test /musl-libc/test
RUN cd musl-libc/test && make && make clean && cd src && diff REPORT.clean REPORT

FROM libtest AS unittest
ADD gdb_allow_autoload /root/.gdbinit
ADD tests /wsp-tests
RUN cd wsp-tests && ./buildtest.sh

FROM unittest AS libc-bench
ADD apps/libc-bench /apps/libc-bench
RUN cd apps/libc-bench && make

FROM libc-bench AS taslock
ADD taslock /taslock
RUN cd taslock && make

FROM taslock AS stamp
ADD apps/stamp /apps/stamp
RUN cd apps/stamp && EXTRA_CFLAGS=' -O3' make

FROM stamp AS memcake
ENV MEMCAKE_DEPS='openssl-dev automake autoconf bison flex perl-utils libtool gettext-dev intltool'
RUN apk add $MEMCAKE_DEPS
ADD apps/memcached /apps/memcached
RUN cd apps/memcached/libevent && autoupdate && ./autogen.sh && \
       CFLAGS="-g" ./configure && make && make install
RUN cd apps/memcached && ./configure && make
ADD apps/memcached-1.6.17 /apps/memcached-1.6.17
RUN cd apps/memcached-1.6.17 && ./autogen.sh && ./configure && make
ADD apps/libmemcached /apps/libmemcached
RUN cd apps/libmemcached && ./bootstrap.sh autoreconf \
	&& ./configure --enable-memaslap \
	&& make -j libmemcached/csl/parser.h && make -j clients/memaslap

FROM memcake AS snek
ENV SNEK_DEPS='zlib-dev libffi-dev libjpeg libjpeg-turbo-dev openblas-dev lapack-dev python3-dev'
RUN apk add $SNEK_DEPS
ADD apps/cpython /apps/cpython
RUN cd apps/cpython && CFLAGS="-g" ./configure --enable-optimizations --with-lto && make -j && make install
#RUN cd apps/cpython && CFLAGS="-O0 -g" ./configure --with-assertions --with-pydebug && make -j && make install
ADD py-tests apps/cpython/wsp-tests
RUN cd apps/cpython/wsp-tests && python3 -m compileall -q **/*.py && rm -rf **/*.pyc **/__pycache__

FROM snek AS sneksit
#ENV PIP_INSTALL='pip3 install --install-option="--jobs=8"'
ENV PIP_INSTALL='pip3 install'
RUN pip3 install --upgrade pip
ENV SNEKSIT_PYDEPS='chameleon pyaes django dulwich genshi'
RUN $PIP_INSTALL $SNEKSIT_PYDEPS
ADD apps/sneksit /apps/sneksit
RUN cd apps/sneksit && python3 -m compileall -q tm_*.py && rm -rf *.pyc __pycache__

## this also includes atlas and mnemnosyne
#FROM sneksit AS clobber-pmdk
#ENV CLOBBER_PMDK_DEPS='wget xz-dev cmake numactl-dev libevent-dev llvm clang scons libelf elfutils-dev libconfig-dev libexecinfo-dev pkgconf ruby boost1.75 boost1.75-dev libdwarf-dev musl-obstack-dev ndctl-dev fts-dev'
#RUN apk add $CLOBBER_PMDK_DEPS
#ENV CLOBBER_PMDK_PYDEPS='pandas'
#RUN alias make="make -j" && $PIP_INSTALL $CLOBBER_PMDK_PYDEPS && unalias make
## pmdk
#ADD clobber-pmdk/pmdk /clobber-pmdk/pmdk
#RUN cd clobber-pmdk/pmdk && ./build.sh
## mnemosyne
ADD clobber-pmdk/mnemosyne-gcc /clobber-pmdk/mnemosyne-gcc
#RUN cd clobber-pmdk/mnemosyne-gcc && ./build.sh
## llvm, w/clobber passes
#ADD clobber-pmdk/llvm /clobber-pmdk/llvm
#ADD clobber-pmdk/passes /clobber-pmdk/passes
#ADD clobber-pmdk/build_clobberpass.sh /clobber-pmdk/
#RUN cd clobber-pmdk && ./build_clobberpass.sh
#ADD clobber-pmdk/rollinlineclang clobber-pmdk/clobberlogclang /clobber-pmdk
## atlas
#ADD clobber-pmdk/atlas /clobber-pmdk/atlas
#RUN cd clobber-pmdk/atlas && ./build.sh
## taslock
#ADD clobber-pmdk/taslock /clobber-pmdk/taslock
#RUN cd clobber-pmdk/taslock && make clean && make
## build clobber runtime and clobber apps (memcached, stamp)
#ADD clobber-pmdk/apps /clobber-pmdk/apps
#RUN cd clobber-pmdk/apps && ./build_runtime.sh
#RUN cd clobber-pmdk/apps && ./build_memcached.sh mutex pmdk && ./build_memcached.sh mutex clobber
#RUN cd clobber-pmdk/apps/stamp/vacation && ./build.sh rbtree pmdk && ./build.sh rbtree clobber
#RUN cd clobber-pmdk/apps/stamp/yada && make

# add top-level runners
ADD apps/run_all.sh apps/run_memcache.sh apps/run_new_memcache.sh apps/run_all.sh /apps/
#ADD clobber-pmdk/run_all.sh /clobber-pmdk/
ADD ./run_all.sh /
