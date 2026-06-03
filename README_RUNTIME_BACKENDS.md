# SCML Runtime Backends & Build Requirements

Este documento describe los requisitos para las APIs nativas que el runtime builtin usa directamente. El runtime ya no pre-registra backends de relleno ni importa cabeceras de Vulkan, DirectX, Metal, OpenGLES, audio o codecs de imagen. Esas integraciones grandes deben hacerse mediante FFI (`ffi.load` / `ffi.call_name`) o mediante módulos host registrados explícitamente por una aplicación.

## Enfoque

- La VM SCML permanece mínima.
- `runtime/scml_runtime_modules.c` solo importa y compila handlers directos para SDL2 y OpenGL cuando se habilitan sus flags.
- Las librerías SCML (`.scmlh`) consumen handlers existentes vía `CALL_NATIVE module.function`.
- Los ports de juegos y APIs grandes pueden cargar sus DLL/SO/DYLIB con la FFI sin inflar el runtime builtin.

## Flags de compilación relevantes

- `SCML_USE_SDL2`: habilita handlers reales de ventana/input SDL2.
- `SCML_USE_OPENGL`: habilita handlers reales de GPU OpenGL.

## Linux (Debian/Ubuntu)

Instalar para runtime SDL2/OpenGL y ejemplos FFI:

```sh
sudo apt update
sudo apt install -y build-essential pkg-config libsdl2-dev libgl1-mesa-dev libglfw3-dev
```

## Arch Linux

```sh
sudo pacman -S --needed base-devel pkgconf sdl2 mesa glfw
```

## macOS (Homebrew)

```sh
brew install sdl2 glfw
```

Nota: OpenGL está deprecado en macOS, pero los ejemplos FFI pueden seguir usando librerías disponibles en el sistema o backends host propios.

## Windows (MSYS2 UCRT64 / MinGW64)

Para el runtime y el port `hola.html` con SDL2/OpenGL:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libffi mingw-w64-ucrt-x86_64-SDL2 make pkgconf
```

Para la variante MinGW64 clásica, usa los paquetes equivalentes `mingw-w64-x86_64-*`.

## Librerías SCML recomendadas

Crear `.scmlh` por dominio, pero solo apuntando a handlers reales o a FFI explícita:

- `gfx.scmlh` (gpu/window cuando SDL2/OpenGL está compilado, o FFI para APIs externas).
- `io.scmlh` (file/read_txt/write_txt).
- `net.scmlh` (open_socket/send_data/receive_data).

## Ejemplo de compilación con flags

```sh
make CFLAGS="-std=c99 -O2 -Wall -Wextra -pedantic -DSCML_USE_SDL2 -DSCML_USE_OPENGL"
```

## Ejemplo OpenGL por FFI: cubo rotando

El ejemplo `examples/opengl_ffi_rotating_cube/` demuestra una integración directa por FFI: SCML carga GLFW/OpenGL con `ffi.load` y llama símbolos nativos con `ffi.call_name`. No hay host C++ dedicado ni wrapper OpenGL específico en la VM. Para MSYS2 UCRT64 abre la shell UCRT64 y usa `mingw-w64-ucrt-x86_64-glfw`; en Linux instala `libglfw3` y `libgl1`.

Comandos directos:

```sh
examples/opengl_ffi_rotating_cube/build_linux.sh
examples/opengl_ffi_rotating_cube/build_msys2_ucrt64.sh
```
