# Game Port de `hola.html` a SCML puro + FFI SDL2/OpenGL

Este directorio contiene una conversión práctica del juego `hola.html` ("Recolector en Terreno 3D") a SCML ejecutado por la VM del repositorio. El port no usa Three.js, navegador, wrappers C ni glue externo: carga SDL2 y OpenGL con `ffi.load`, consulta teclado/mouse con SDL2 y renderiza con OpenGL inmediato mediante `ffi.call_name`.

## Archivos

- `linux_hola_sdl2_opengl_ffi.scml`: Linux (`libSDL2-2.0.so.0` + `libGL.so.1`).
- `msys2_ucrt64_hola_sdl2_opengl_ffi.scml`: Windows MSYS2 UCRT64/MinGW64 (`SDL2.dll` + `opengl32.dll`).
- `build_linux.sh`: compila el runtime y el bytecode Linux.
- `build_msys2_ucrt64.sh`: compila `bin/scml.exe` y el bytecode Windows desde una shell MSYS2 UCRT64/MinGW64.

## Controles actuales

El juego sigue siendo jugable completo con teclado y ahora acepta mouse para cámara en Linux y Windows:

- `W/A/S/D`: movimiento relativo a la cámara.
- `Flechas`: cámara por yaw/pitch si no quieres usar mouse.
- `Mouse`: cámara libre con modo relativo de SDL2; el cursor se oculta durante la partida y vuelve al cerrar.
- `Espacio`: salto con gravedad y terreno procedural.
- `R`: reiniciar partida y regenerar coleccionables/rocas.
- `Esc`: cerrar con limpieza de memoria FFI, contexto GL y ventana SDL2.

## Features trasladadas/mejoradas

- Terreno procedural con altura compuesta por `sin/cos` en SCML.
- Coleccionables naranjas con memoria nativa (`ffi.alloc_array`, `ffi.array_read`, `ffi.array_write`).
- Puntuación y reset de partida.
- Rocas/obstáculos, sombras volátiles, luces erráticas, stalker desbloqueable y monolito celeste desbloqueable.
- Niebla OpenGL (`glEnable(GL_FOG)`, `glFogi`, `glFogf`) y color de cielo variable por altura.
- Entrada nativa robusta: `SDL_PumpEvents`, `SDL_GetKeyboardState`, `SDL_GetRelativeMouseState`, buffers FFI para `int*` y restauración de `SDL_SetRelativeMouseMode` al salir.
- Limpieza robusta de recursos en la ruta normal y rutas de error: arrays nativos, buffers de mouse, contexto GL, ventana y subsistema SDL2.

## Compilar en Linux

```sh
./examples/sdl2_opengl_hola_game/build_linux.sh
```

## Ejecutar en Linux

```sh
./bin/scml run examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin
```

## Compilar en Windows MSYS2 UCRT64/MinGW64

Instala dependencias desde una shell MSYS2 UCRT64 o MinGW64:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libffi mingw-w64-ucrt-x86_64-SDL2 make pkgconf
./examples/sdl2_opengl_hola_game/build_msys2_ucrt64.sh
```

Si usas una shell MinGW64 clásica, instala los paquetes equivalentes `mingw-w64-x86_64-*` y ejecuta el mismo script desde esa shell.

## Ejecutar en Windows MSYS2

```sh
bin/scml.exe run .scml/msys2_ucrt64_hola_sdl2_opengl_ffi.scmlbin
```

Requisitos: runtime SCML compilado con libffi, SDL2, OpenGL y una sesión gráfica disponible. En CI/headless se recomienda compilar/checkear el script sin ejecutar la ventana.
