#!/bin/sh
# Compila o core para Android arm64 com o NDK (Linux/macOS).  Uso: NDK=/caminho/do/ndk ./build_android.sh
set -e
: "${NDK:?defina NDK=/caminho/do/android-ndk}"
HOST=$(ls "$NDK/toolchains/llvm/prebuilt")
CC="$NDK/toolchains/llvm/prebuilt/$HOST/bin/aarch64-linux-android24-clang"
$CC -shared -fPIC -O3 -ffast-math -fvisibility=hidden -Wl,-z,max-page-size=16384 \
    -o retro8_voxel_libretro_android.so retro8_voxel.c -lm -ldl
echo "Gerado: retro8_voxel_libretro_android.so"
