#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
mkdir -p .scml
make bin/scml.exe
bin/scml.exe compile examples/opengl_ffi_rotating_cube/msys2_ucrt64_opengl_ffi_rotating_cube.scml .scml/msys2_ucrt64_opengl_ffi_rotating_cube.scmlbin
echo "[opengl-ffi] Run: bin/scml.exe run .scml/msys2_ucrt64_opengl_ffi_rotating_cube.scmlbin"
