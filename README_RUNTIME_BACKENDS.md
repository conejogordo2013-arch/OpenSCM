# SCML Runtime Backends & Build Requirements

Este documento describe requisitos para compilar SCML con APIs reales (gráficos/audio/ventana/texto/imágenes) mediante módulos nativos.

## Enfoque

- La VM SCML permanece mínima.
- Las APIs reales viven en `runtime/scml_runtime_modules.c` y módulos/backends.
- Las librerías SCML (`.scmlh`) consumen estas APIs vía `CALL_NATIVE module.function`.

## Flags de compilación relevantes

- `SCML_USE_SDL2`
- `SCML_USE_OPENGL`
- `SCML_USE_OPENGLES`
- `SCML_USE_VULKAN`
- `SCML_USE_D3D11`
- `SCML_USE_D3D12`
- `SCML_USE_METAL`

## Linux (Debian/Ubuntu)

Instalar:

```sh
sudo apt update
sudo apt install -y build-essential pkg-config libsdl2-dev libglfw3-dev libgl1-mesa-dev libgles2-mesa-dev libvulkan-dev libx11-dev
```

## Arch Linux

```sh
sudo pacman -S --needed base-devel pkgconf glfw sdl2 mesa vulkan-headers vulkan-icd-loader libx11
```

## macOS (Homebrew)

```sh
brew install sdl2 molten-vk
```

Notas:
- OpenGL está deprecado pero disponible.
- Metal se habilita con toolchain de Xcode + SDK nativo.

## Windows (MSYS2 / MinGW64)

```sh
pacman -Syu
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-pkgconf mingw-w64-x86_64-glfw mingw-w64-x86_64-SDL2 mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader
```

Para D3D11/D3D12 se requiere Windows SDK (MSVC/clang-cl entorno adecuado).

## Librerías SCML recomendadas

Crear `.scmlh` por dominio:

- `gfx.scmlh` (gpu/window/image)
- `audio.scmlh` (audio)
- `io.scmlh` (file/read_txt/write_txt)
- `net.scmlh` (open_socket/send_data/receive_data)

Estas librerías encapsulan `CALL_NATIVE` y dejan scripts SCML limpios.

## Ejemplo de compilación con flags

```sh
make CFLAGS="-std=c99 -O2 -Wall -Wextra -pedantic -DSCML_USE_SDL2 -DSCML_USE_OPENGL -DSCML_USE_VULKAN"
```


## Ejemplo OpenGL por FFI: cubo rotando

El ejemplo `examples/opengl_ffi_rotating_cube/` demuestra una integración directa por FFI: SCML carga GLFW/OpenGL con `ffi.load` y llama símbolos nativos con `ffi.call_name`. No hay host C++ dedicado ni wrapper OpenGL específico en la VM. Para MSYS2 UCRT64 abre la shell UCRT64 y usa `mingw-w64-ucrt-x86_64-glfw`; en Linux instala `libglfw3` y `libgl1`.

Comandos directos:

```sh
examples/opengl_ffi_rotating_cube/build_linux.sh
examples/opengl_ffi_rotating_cube/build_msys2_ucrt64.sh
```
