# SCML SDL2/OpenGL Spawn Cubes Prank

Ejemplo visual escrito en **SCML puro** usando FFI dinámico a SDL2 y OpenGL: no hay wrapper C/C++ ni binario nativo auxiliar. Abre una ventana con un botón azul superior izquierdo (**SPAWNEAR CUBO**, indicado también en el título de la ventana); cada click dentro del botón spawnea un cubo OpenGL y aumenta el contador.

Cuando el contador llega a **100**, el programa construye estructuras `SDL_MessageBoxData` con la FFI de SCML y muestra el diálogo de broma:

```text
ConejoGruñe.dll Not Encountered
You Are Hacked :)
```

El diálogo tiene dos botones:

- **Cancelar**: termina el juego.
- **Fix**: escribe `trolleado.txt` con el mensaje `Trolleado Pero bueno`, muestra una confirmación y reinicia el contador.

## Compilar en Linux

```sh
./examples/sdl2_opengl_spawn_cubes_prank/build_linux.sh
bin/scml run bin/scml_spawn_cubes_prank_linux.scmlbin
```

Requiere SDL2 y OpenGL disponibles como `libSDL2-2.0.so.0` y `libGL.so.1`.

## Compilar en MSYS2 UCRT64

```sh
./examples/sdl2_opengl_spawn_cubes_prank/build_msys2_ucrt64.sh
bin/scml run bin/scml_spawn_cubes_prank_msys2_ucrt64.scmlbin
```

Requiere `SDL2.dll` y `opengl32.dll` accesibles desde el entorno MSYS2/UCRT64.
