#!/bin/sh
# Teste no PC (x86): usa um core PICO-8 falso e salva out.ppm com o quadro renderizado.
set -e
cd "$(dirname "$0")/.."; mkdir -p /tmp/r8v
gcc -shared -fPIC -O2 -fvisibility=hidden -o /tmp/r8v/retro8_voxel_libretro_android.so retro8_voxel.c -lm -ldl
gcc -shared -fPIC -O2 -o /tmp/r8v/retro8_libretro_android.so test/fake_inner.c -lm
gcc -O2 -o /tmp/r8v/host test/test_host.c -ldl
cd /tmp/r8v && ./host 40 "$@" && echo "saida: /tmp/r8v/out.ppm"
