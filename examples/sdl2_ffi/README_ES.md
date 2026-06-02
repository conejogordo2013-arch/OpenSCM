# Ejemplos SDL2 + FFI para crear una ventana

Estos ejemplos muestran cómo crear una ventana SDL2 desde SCML usando FFI directa. Son ejemplos básicos: cargan SDL2, declaran las funciones C necesarias, crean una ventana, esperan 2 segundos y cierran SDL.

## Linux

Archivo: [`linux_sdl2_window_ffi.scml`](linux_sdl2_window_ffi.scml)

Dependencias típicas:

```sh
# Debian/Ubuntu
sudo apt install libsdl2-2.0-0

# Fedora
sudo dnf install SDL2

# Arch
sudo pacman -S sdl2
```

Compilar y ejecutar desde la raíz del repositorio:

```sh
make bin/scml
bin/scml compile examples/sdl2_ffi/linux_sdl2_window_ffi.scml .scml/linux_sdl2_window_ffi.scmlbin
bin/scml run .scml/linux_sdl2_window_ffi.scmlbin
```

El ejemplo carga `libSDL2-2.0.so.0` y declara `SDL_Init`, `SDL_CreateWindow`, `SDL_Delay`, `SDL_DestroyWindow` y `SDL_Quit`.

## MSYS2 UCRT64

Archivo: [`msys2_ucrt64_sdl2_window_ffi.scml`](msys2_ucrt64_sdl2_window_ffi.scml)

En la terminal **MSYS2 UCRT64**:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-libffi make gcc
make bin/scml
bin/scml compile examples/sdl2_ffi/msys2_ucrt64_sdl2_window_ffi.scml .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin
bin/scml run .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin
```

El ejemplo carga `SDL2-2.0-0`, que en UCRT64 normalmente resuelve a `/ucrt64/bin/SDL2-2.0-0.dll` si la terminal tiene el `PATH` correcto.

## Notas

- Son ejemplos intencionalmente mínimos; no hacen loop de eventos con `SDL_PollEvent`.
- Si el entorno no tiene display gráfico, la ejecución puede fallar aunque la compilación SCML sea correcta.
- Para programas reales conviene envolver SDL2 en una librería C pequeña o usar los módulos runtime nativos de OpenSCM cuando estén compilados con SDL2.
