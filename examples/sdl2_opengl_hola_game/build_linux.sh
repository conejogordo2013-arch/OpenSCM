#!/usr/bin/env sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$ROOT"
make core
./bin/scml compile examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scml examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin
printf '%s\n' "Compilado: examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin"
printf '%s\n' "Ejecuta con display SDL2/OpenGL: ./bin/scml run examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin"
