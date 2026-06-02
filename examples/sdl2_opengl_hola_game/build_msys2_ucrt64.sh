#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
mkdir -p .scml
make bin/scml.exe
bin/scml.exe compile examples/sdl2_opengl_hola_game/msys2_ucrt64_hola_sdl2_opengl_ffi.scml .scml/msys2_ucrt64_hola_sdl2_opengl_ffi.scmlbin
echo "[hola-game] Ejecuta desde MSYS2 UCRT64/MinGW64 con SDL2 en PATH: bin/scml.exe run .scml/msys2_ucrt64_hola_sdl2_opengl_ffi.scmlbin"
