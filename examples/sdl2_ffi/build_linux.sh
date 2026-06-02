#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! pkg-config --exists sdl2; then
  echo "[sdl2-ffi] SDL2 development files not found." >&2
  echo "[sdl2-ffi] Install them first, for example: sudo apt install libsdl2-dev" >&2
  exit 1
fi

mkdir -p .scml examples/sdl2_ffi
${CC:-cc} -shared -fPIC \
  -o examples/sdl2_ffi/libscml_sdl2_window.so \
  examples/sdl2_ffi/scml_sdl2_window.c \
  $(pkg-config --cflags --libs sdl2)

make bin/scml
bin/scml compile examples/sdl2_ffi/linux_sdl2_window_ffi.scml .scml/linux_sdl2_window_ffi.scmlbin

echo "[sdl2-ffi] Built examples/sdl2_ffi/libscml_sdl2_window.so"
echo "[sdl2-ffi] Run: bin/scml run .scml/linux_sdl2_window_ffi.scmlbin"
