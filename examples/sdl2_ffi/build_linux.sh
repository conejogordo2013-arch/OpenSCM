#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! ldconfig -p 2>/dev/null | grep -q 'libSDL2-2.0.so.0' && ! pkg-config --exists sdl2 2>/dev/null; then
  echo "[sdl2-ffi] SDL2 runtime library was not detected; the SCML script will still compile." >&2
  echo "[sdl2-ffi] Install it before running the window: sudo apt install libsdl2-2.0-0" >&2
  echo "[sdl2-ffi] Development headers are not required because SCML calls SDL2 directly by symbol." >&2
fi

mkdir -p .scml
make bin/scml
bin/scml compile examples/sdl2_ffi/linux_sdl2_window_ffi.scml .scml/linux_sdl2_window_ffi.scmlbin

echo "[sdl2-ffi] Compiled direct dynamic-FFI SDL2 example."
echo "[sdl2-ffi] Run: bin/scml run .scml/linux_sdl2_window_ffi.scmlbin"
