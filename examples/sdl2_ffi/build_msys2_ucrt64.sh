#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v pacman >/dev/null 2>&1; then
  echo "[sdl2-ffi] This helper is intended for MSYS2 UCRT64." >&2
fi

if ! command -v SDL2.dll >/dev/null 2>&1; then
  echo "[sdl2-ffi] Ensure SDL2.dll is available in PATH before running the compiled script." >&2
  echo "[sdl2-ffi] Install it with: pacman -S --needed mingw-w64-ucrt-x86_64-SDL2" >&2
fi

mkdir -p .scml
make bin/scml
bin/scml compile examples/sdl2_ffi/msys2_ucrt64_sdl2_window_ffi.scml .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin

echo "[sdl2-ffi] Compiled direct dynamic-FFI SDL2 example."
echo "[sdl2-ffi] Run: bin/scml run .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin"
