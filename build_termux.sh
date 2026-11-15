#!/data/data/com.termux/files/usr/bin/sh
# Compila direto no celular, dentro do Termux (arm64).  Uso: sh build_termux.sh
set -e
command -v clang >/dev/null || pkg install -y clang
clang -shared -fPIC -O3 -ffast-math -fvisibility=hidden \
    -o retro8_voxel_libretro_android.so retro8_voxel.c -lm -ldl
echo "Gerado: retro8_voxel_libretro_android.so"
