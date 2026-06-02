# Ejemplos SDL2 + FFI para crear una ventana real

Estos ejemplos ya no son solo una lista decorativa de llamadas: incluyen una librería C mínima (`scml_sdl2_window.c`) que **compila contra SDL2** y expone una función FFI real:

```c
int32_t scml_sdl2_open_window_ms(int32_t width, int32_t height, const char *title, int32_t milliseconds);
```

El script SCML carga esa librería, declara la función por FFI y la ejecuta para abrir una ventana 640x360 durante 2 segundos.

## Linux

Archivo SCML: [`linux_sdl2_window_ffi.scml`](linux_sdl2_window_ffi.scml)

Instala las dependencias de desarrollo de SDL2:

```sh
# Debian/Ubuntu
sudo apt install libsdl2-dev pkg-config

# Fedora
sudo dnf install SDL2-devel pkgconf-pkg-config

# Arch
sudo pacman -S sdl2 pkgconf
```

Compila la librería nativa, compila el SCML y luego ejecútalo:

```sh
examples/sdl2_ffi/build_linux.sh
bin/scml run .scml/linux_sdl2_window_ffi.scmlbin
```

El script genera `examples/sdl2_ffi/libscml_sdl2_window.so` y compila `linux_sdl2_window_ffi.scml`.

## MSYS2 UCRT64

Archivo SCML: [`msys2_ucrt64_sdl2_window_ffi.scml`](msys2_ucrt64_sdl2_window_ffi.scml)

En la terminal **MSYS2 UCRT64** instala dependencias:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-libffi make gcc pkgconf
```

Compila la DLL, compila el SCML y luego ejecútalo:

```sh
examples/sdl2_ffi/build_msys2_ucrt64.sh
bin/scml run .scml/msys2_ucrt64_sdl2_window_ffi.scmlbin
```

El script genera `examples/sdl2_ffi/scml_sdl2_window.dll` y compila `msys2_ucrt64_sdl2_window_ffi.scml`.

## Notas importantes

- La smoke suite compila los scripts SCML, pero no ejecuta la ventana para no exigir display gráfico en CI/headless.
- Si no tienes entorno gráfico (`DISPLAY`, Wayland/X11, Windows desktop), la función puede devolver un código negativo aunque compile correctamente.
- Códigos de retorno: `1` = OK, `-1` = tamaño inválido, `-2` = `SDL_InitSubSystem` falló, `-3` = `SDL_CreateWindow` falló.
