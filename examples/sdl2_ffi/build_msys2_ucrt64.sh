#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! pkg-config --exists sdl2; then
  echo "[sdl2-ffi] SDL2 development files not found in MSYS2 UCRT64." >&2
  echo "[sdl2-ffi] Install them with:" >&2
  echo "  pacman -S --needed mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-libffi make gcc pkgconf" >&2
  exit 1
fi

mkdir -p .scml examples/sdl2_ffi
${CC:-gcc} -shared \
  -o examples/sdl2_ffi/scml_sdl2_window.dll \
  examples/sdl2_ffi/scml_sdl2_window.c \
  $(pkg-config --cflags --libs sdl2)

make bin/scml
bin/scml compile examples/sdl2_ffi/msys2_ucrt64_sdl2_window_ffi.scml .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin

echo "[sdl2-ffi] Built examples/sdl2_ffi/scml_sdl2_window.dll"
echo "[sdl2-ffi] Run: bin/scml run .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin"
