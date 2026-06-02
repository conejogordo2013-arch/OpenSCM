# Cubo giratorio OpenGL con SCML + FFI

Este ejemplo no usa un host C++ propio ni un wrapper OpenGL especial. El archivo SCML carga las librerías dinámicas con `ffi.load` y llama directamente a GLFW/OpenGL con `ffi.call_name`.

## Linux

Dependencias típicas:

```bash
sudo apt install build-essential libglfw3 libgl1
```

Compilar el bytecode SCML:

```bash
examples/opengl_ffi_rotating_cube/build_linux.sh
```

Ejecutar:

```bash
bin/scml run .scml/linux_opengl_ffi_rotating_cube.scmlbin
```

## MSYS2 UCRT64

Abre la shell **MSYS2 UCRT64** e instala:

```bash
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glfw
```

Compilar el bytecode SCML:

```bash
examples/opengl_ffi_rotating_cube/build_msys2_ucrt64.sh
```

Ejecutar:

```bash
bin/scml.exe run .scml/msys2_ucrt64_opengl_ffi_rotating_cube.scmlbin
```

## Qué demuestra

- SCML controla la ventana, el loop, la cámara, el clear, la rotación y los vértices del cubo.
- FFI resuelve símbolos reales (`glfwInit`, `glfwCreateWindow`, `glClear`, `glRotatef`, `glVertex3f`, etc.) en runtime.
- No hay backend C++ dedicado: si una función nativa existe en la librería cargada, SCML puede invocarla con una firma FFI.
