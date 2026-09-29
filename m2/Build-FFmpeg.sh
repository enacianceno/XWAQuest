#!/usr/bin/env bash
set -euo pipefail
# Execute with Git for Windows bash. All compiler programs are NDK Windows
# executables targeting Android; make/bash alone are host programs.
cd "$(dirname "$0")"
m2_root=$(pwd)
ndk_bin="${M2_NDK_BIN:-/c/Users/enaci/AppData/Local/Android/Sdk/ndk/28.2.13676358/toolchains/llvm/prebuilt/windows-x86_64/bin}"
host_bin="$m2_root/host/llvm-mingw-20260908-ucrt-x86_64/bin"
export PATH="$m2_root/host/msys-make/usr/bin:$host_bin:$ndk_bin:$PATH"
mkdir -p build-ffmpeg prefix/android-arm64 evidence
cd build-ffmpeg
../deps/ffmpeg/configure \
    --prefix="$(cygpath -m "$m2_root/prefix/android-arm64")" \
    --target-os=android --arch=aarch64 --enable-cross-compile \
    --host-cc="$(cygpath -m "$host_bin/clang.exe")" \
    --cc="$(cygpath -m "$ndk_bin/clang.exe") --target=aarch64-linux-android29" \
    --cxx="$(cygpath -m "$ndk_bin/clang++.exe") --target=aarch64-linux-android29" \
    --ar="$(cygpath -m "$ndk_bin/llvm-ar.exe")" \
    --ranlib="$(cygpath -m "$ndk_bin/llvm-ranlib.exe")" \
    --strip="$(cygpath -m "$ndk_bin/llvm-strip.exe")" \
    --nm="$(cygpath -m "$ndk_bin/llvm-nm.exe")" \
    --enable-shared --disable-static --enable-pic \
    --disable-autodetect --enable-zlib --disable-programs --disable-doc \
    --disable-avdevice --disable-avfilter --disable-postproc \
    --disable-encoders --disable-muxers \
    --extra-ldflags=-Wl,-z,max-page-size=16384 \
    2>&1 | tee ../evidence/ffmpeg-configure.log
make -j8 2>&1 | tee ../evidence/ffmpeg-build.log
make install 2>&1 | tee ../evidence/ffmpeg-install.log
