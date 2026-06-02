#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
mkdir -p .scml
make bin/scml
bin/scml compile examples/opengl_ffi_rotating_cube/linux_opengl_ffi_rotating_cube.scml .scml/linux_opengl_ffi_rotating_cube.scmlbin
echo "[opengl-ffi] Run: bin/scml run .scml/linux_opengl_ffi_rotating_cube.scmlbin"
