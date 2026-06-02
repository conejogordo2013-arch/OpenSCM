# Ejemplos SDL2 + FFI dinámico puro

Estos ejemplos muestran el cambio real del FFI: **SCML llama directamente a SDL2 por símbolo**.
No hay wrapper C, no hay funciones `scml_lib_*`, no hay headers de SDL2 importados por SCML y no hay glue code por cada función.

El flujo es:

1. `ffi.load` carga una sola vez la librería dinámica (`libSDL2-2.0.so.0` en Linux, `SDL2.dll` en MSYS2 UCRT64).
2. `ffi.call_name` resuelve `SDL_Init`, `SDL_CreateWindow`, `SDL_Delay`, `SDL_DestroyWindow` y `SDL_QuitSubSystem` por nombre en runtime.
3. Cada símbolo resuelto queda en la cache global del FFI, así que llamadas posteriores no vuelven a hacer `dlsym`/`GetProcAddress`.
4. La firma se pasa como texto en la llamada SCML; no se declara ni se envuelve en C.

## Linux

Archivo SCML: [`linux_sdl2_window_ffi.scml`](linux_sdl2_window_ffi.scml)

Instala la librería runtime de SDL2 si no está presente:

```sh
# Debian/Ubuntu
sudo apt install libsdl2-2.0-0

# Fedora
sudo dnf install SDL2

# Arch
sudo pacman -S sdl2
```

Compila el script SCML y luego ejecútalo:

```sh
examples/sdl2_ffi/build_linux.sh
bin/scml run .scml/linux_sdl2_window_ffi.scmlbin
```

## MSYS2 UCRT64

Archivo SCML: [`msys2_ucrt64_sdl2_window_ffi.scml`](msys2_ucrt64_sdl2_window_ffi.scml)

En la terminal **MSYS2 UCRT64** instala SDL2:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-SDL2 make gcc pkgconf
```

Compila el script SCML y luego ejecútalo:

```sh
examples/sdl2_ffi/build_msys2_ucrt64.sh
bin/scml run .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin
```

## Notas importantes

- La smoke suite compila los scripts SCML, pero no ejecuta la ventana para no exigir display gráfico en CI/headless.
- Si no tienes entorno gráfico (`DISPLAY`, Wayland/X11 o Windows desktop), SDL2 puede fallar al crear la ventana aunque el FFI compile correctamente.
- El ejemplo imprime `SDL2 window OK via dynamic FFI` si pudo abrir y cerrar la ventana.
- Si falla, imprime `ffi.last_error`; cuando SDL2 devuelve error propio sin fallo de resolución de símbolo, consulta tu entorno gráfico/SDL2.
