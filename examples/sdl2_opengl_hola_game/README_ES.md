# Game Port de `hola.html` a SCML puro + FFI SDL2/OpenGL

Este directorio contiene una conversión práctica del juego `hola.html` ("Recolector en Terreno 3D") a SCML ejecutado por la VM del repositorio. El port no usa Three.js, navegador, wrappers C ni glue externo: carga `libSDL2-2.0.so.0` y `libGL.so.1` con `ffi.load`, consulta teclado con `SDL_GetKeyboardState` y renderiza con OpenGL inmediato mediante `ffi.call_name`.

## Controles actuales

Por petición, el juego queda limitado a teclado por ahora:

- `W/A/S/D`: movimiento relativo a la cámara.
- `Flechas`: cámara por yaw/pitch.
- `Espacio`: salto con gravedad y terreno procedural.
- `R`: reiniciar partida y regenerar coleccionables/rocas.
- `Esc`: cerrar con limpieza de memoria FFI, contexto GL y ventana SDL2.

## Features trasladadas/mejoradas

- Terreno procedural con altura compuesta por `sin/cos` en SCML.
- Coleccionables naranjas con memoria nativa (`ffi.alloc_array`, `ffi.array_read`, `ffi.array_write`).
- Puntuación y reset de partida.
- Rocas/obstáculos, sombras volátiles, luces erráticas, stalker desbloqueable y monolito celeste desbloqueable.
- Niebla OpenGL (`glEnable(GL_FOG)`, `glFogi`, `glFogf`) y color de cielo variable por altura.
- Limpieza robusta de recursos en la ruta normal y rutas de error.

## Compilar

```sh
./examples/sdl2_opengl_hola_game/build_linux.sh
```

## Ejecutar

```sh
./bin/scml run examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin
```

Requisitos: runtime SCML compilado con libffi, SDL2, OpenGL y una sesión gráfica disponible. En CI/headless se recomienda compilar/checkear el script sin ejecutar la ventana.
