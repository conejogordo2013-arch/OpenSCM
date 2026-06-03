#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"
make bin/scml >/dev/null
mkdir -p bin
bin/scml compile examples/sdl2_opengl_spawn_cubes_prank/linux_sdl2_opengl_spawn_cubes_prank.scml bin/scml_spawn_cubes_prank_linux.scmlbin
echo "Run: bin/scml run bin/scml_spawn_cubes_prank_linux.scmlbin"
