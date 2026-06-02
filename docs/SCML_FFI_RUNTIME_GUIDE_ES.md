# Guía completa de SCML moderno, runtime hostless y FFI

Esta guía resume el estado estable del lenguaje SCML y documenta cómo usar sus capacidades avanzadas sin depender de glue nativo adicional.

## 1. Objetivo de estabilidad

SCML mantiene dos capas compatibles:

1. **Núcleo legacy SCM/opcode**: etiquetas `:MAIN`, opcodes numéricos, saltos `@LABEL`, `CALL`, `return` y `0001:` siguen siendo la base estable.
2. **Superficie moderna**: `script`, `fn`, `task`, `let`, `if`, `while`, `for`, `break`, `continue`, `spawn`, `co_await`, imports `use`, macros con llaves y helpers estándar bajan al mismo bytecode.

La regla de diseño es que una mejora moderna no debe romper scripts legacy existentes. Cuando se añaden APIs nuevas, se exponen por `CALL_NATIVE` y por macros del stdlib para mantener compatibilidad.

## 2. Runtime hostless recomendado

Los módulos hostless se llaman con `0B31: "modulo.funcion" ...` o mediante macros estándar:

- `runtime.*`: tiempo, ticks, espera y backend info.
- `file.*`: existencia, lectura/escritura, directorios.
- `console.*`: salida, color, cursor y limpieza.
- `input.*`, `window.*`, `gpu.*`, `audio.*`, `net.*`: APIs portables con backend `default` y backends acelerados si se compila con SDL/OpenGL/etc.
- `data.*`: transformación de datos sin host glue.
- `env.*`: variables de entorno del proceso.
- `ffi.*`: carga de librerías nativas, símbolos, memoria, structs, unions, arrays, punteros, vtables y llamadas ABI.

## 3. Módulo `data`

Funciones disponibles:

| Función | Uso | Resultado |
|---|---|---|
| `data.hash32` | `0B31: "data.hash32" texto` | hash FNV-1a de 31 bits positivo |
| `data.url_encode` | `0B31: "data.url_encode" texto` | texto escapado `%XX` |
| `data.url_decode` | `0B31: "data.url_decode" texto` | texto decodificado |
| `data.split` | `0B31: "data.split" texto sep indice` | segmento por índice, o `""` si no existe |
| `data.json_get` | `0B31: "data.json_get" json clave` | valor plano string/int/float/bool |

Macros:

```scml
SCML_DATA_URL_ENCODE("hola mundo", $ENC)
SCML_DATA_URL_DECODE($ENC, $TXT)
SCML_JSON_GET("{\"level\":9000}", "level", $LEVEL)
SCML_DATA_SPLIT("gpu,audio,data", ",", 2, $PART)
SCML_DATA_HASH32("SCML", $HASH)
```

`json_get` está pensado para objetos planos de configuración. Para documentos JSON profundamente anidados, se recomienda usar FFI hacia una biblioteca JSON nativa o preprocesar desde el host.

## 4. Módulo `env`

```scml
SCML_ENV_SET("SCML_MODE", "debug", $OK)
SCML_ENV_GET("SCML_MODE", $MODE)
```

`env.set` devuelve `1` si el sistema aceptó la escritura y `0` si falló.

## 5. FFI avanzado

### 5.1 Reglas de diseño: FFI dinámico por símbolo

El FFI estable de SCML no requiere wrappers manuales:

- No se crean funciones C tipo `scml_lib_*` para cada API externa.
- No se escriben bindings uno por uno ni glue code por función de la librería.
- SCML no incluye headers nativos y solo ve nombres de símbolos y firmas textuales.
- Una librería dinámica se carga una vez con `ffi.load`; los símbolos se resuelven por nombre en runtime.
- Cada símbolo resuelto queda en cache, por lo que las siguientes llamadas reutilizan el puntero de función.

```scml
0B31: "ffi.load" "libscml_ffi_native"

; Llamada dinámica explícita: nombre exportado + retorno + tipos de argumentos + valores.
0B31: "ffi.call_name" "scml_ffi_add_i32" "int" "int,int" 20 22
03E5: $RETVAL

; También se puede resolver el puntero cacheado y llamarlo directamente.
0B31: "ffi.resolve" "scml_ffi_add_i32"
0004: $ADD_PTR $RETVAL
0B31: "ffi.call_ptr" $ADD_PTR "int" "int,int" 100 23
03E5: $RETVAL
```

Tipos soportados en firmas y memoria: `bool`, `int8`, `uint8`, `int16`, `uint16`, `int`, `int32`, `uint32`, `int64`, `uint64`, `size`, `float`, `double`, `pointer`, `string`, `void`.

### 5.2 Llamadas dinámicas por nombre, ABI y aliases opcionales

La forma recomendada es `ffi.call_name name ret arg_spec ...`, porque no registra una función estática ni exige glue C. Para casos donde necesitas forzar ABI o guardar un alias SCML estable hacia un símbolo externo, el runtime también conserva APIs explícitas:

```scml
0B31: "ffi.load" "libscml_ffi_native"
0004: $LIB $RETVAL

0B31: "ffi.call_name_abi" "scml_ffi_add_i32" "cdecl" "int" "int,int" 20 22
03E5: $RETVAL

; Alias opcional: no envuelve C, solo cachea alias -> símbolo nativo resuelto.
0B31: "ffi.bind" $LIB "math.add" "scml_ffi_add_i32" "int" "int,int" "cdecl"
0B31: "ffi.call_name_abi" "math.add" "cdecl" 100 23
03E5: $RETVAL
```

- `ffi.call_name name ret arg_spec ...` resuelve `name`, cachea el puntero y llama con la firma textual indicada.
- `ffi.call_name_abi name abi ret arg_spec ...` hace lo mismo, fijando ABI.
- `ffi.resolve name` devuelve el puntero cacheado o resuelve el símbolo si todavía no estaba en cache.
- `ffi.call_ptr ptr ret arg_spec ...` llama directamente a un puntero de función.
- `ffi.bind handle alias symbol ret arg_spec abi` es opcional y solo guarda un alias SCML hacia un símbolo nativo; no genera C ni wrappers.
- `ffi.stats` devuelve el número de símbolos cacheados para verificar que el runtime no repite búsquedas por llamada.

### 5.3 Punteros y memoria

Funciones principales:

- `ffi.alloc size`, `ffi.alloc_array count type_or_size`, `ffi.realloc ptr size`, `ffi.free ptr`.
- `ffi.read ptr offset type`, `ffi.write ptr offset type value`.
- `ffi.array_read ptr index type`, `ffi.array_write ptr index type value`.
- `ffi.ptr_add ptr offset`, `ffi.ptr_diff lhs rhs`.
- `ffi.null`, `ffi.is_null ptr`, `ffi.ptr_to_int ptr`, `ffi.int_to_ptr value`.
- `ffi.memset ptr byte size`, `ffi.memcpy dst src size`, `ffi.memmove dst src size`, `ffi.memcmp lhs rhs size`.
- `ffi.read_bytes ptr offset size` devuelve bytes en hexadecimal.
- `ffi.write_bytes ptr offset "AABBCC"` escribe bytes desde hexadecimal con espacios opcionales.

Macros útiles:

```scml
SCML_FFI_READ_BYTES($BUF, 0, 4, $HEX)
SCML_FFI_WRITE_BYTES($BUF, 0, "DE AD BE EF", $OK)
SCML_FFI_MEMCMP($A, $B, 4, $CMP)
SCML_FFI_NULL($NULL)
SCML_FFI_IS_NULL($NULL, $IS_NULL)
```

### 5.4 Strings C y UTF-16

- `ffi.cstring "texto"` reserva un `char*` terminado en NUL.
- `ffi.read_cstring ptr` lee hasta NUL con límite de seguridad.
- `ffi.write_cstring ptr offset texto max_bytes` escribe texto truncando y garantizando NUL.
- `ffi.utf16 "texto"` reserva UTF-16 little-endian portable para APIs estilo Windows.
- `ffi.read_utf16 ptr [max_units]` convierte UTF-16 a UTF-8 SCML.

### 5.5 Structs, arrays de structs y unions

```scml
0B31: "ffi.struct_define" "Packet" "uint32:id,uint16:flags,uint8:kind"
0B31: "ffi.alloc_struct_array" "Packet" 3
0004: $PACKETS $RETVAL
0B31: "ffi.struct_array_write" $PACKETS 1 "Packet" "id" 12345
0B31: "ffi.struct_array_read" $PACKETS 1 "Packet" "id"
```

Para campos array fijos:

```scml
0B31: "ffi.struct_define" "Header" "uint32:id,uint8[4]:tag,uint16[3]:scores"
0B31: "ffi.struct_field_write" $H "Header" "scores" 2 900
```

Para unions:

```scml
0B31: "ffi.union_begin" "NumberBits"
0B31: "ffi.struct_field" "NumberBits" "as_i32" "int"
0B31: "ffi.struct_field" "NumberBits" "as_u64" "uint64"
0B31: "ffi.struct_finish" "NumberBits"
```

### 5.6 VTables y ABIs

- `ffi.abi_supported "cdecl"` comprueba soporte.
- `ffi.call_ptr ptr ret args...` llama a un puntero de función.
- `ffi.call_ptr_abi ptr abi ret arg_spec args...` fuerza ABI.
- `ffi.vtable_call object method_index ret arg_spec args...` inyecta `this` automáticamente.
- `ffi.vtable_call_raw` no inyecta `this`.

## 6. Prácticas para evitar bugs

- Libera todo bloque devuelto por `ffi.alloc*`, `ffi.cstring` o `ffi.utf16` con `ffi.free`.
- Usa `ffi.sizeof_block` durante depuración para confirmar tamaños gestionados por FFI.
- Prefiere `ffi.write_bytes` para fixtures binarios y `ffi.write` para tipos escalares.
- Define structs una vez y consulta `ffi.struct_size`, `ffi.struct_align` y `ffi.struct_offset` si necesitas interoperar con C.
- Ejecuta `bash tests/smoke_suite.sh` antes de publicar cambios del lenguaje.
- Mantén scripts legacy y modernos en la misma ruta de compilación: si una feature moderna no baja a bytecode estable, debe ser metadata explícita o macro stdlib.

## 7. Ejemplo completo mínimo

Ver [`examples/universal_runtime_data.scml`](../examples/universal_runtime_data.scml) para `data`/`env` y la smoke suite para casos FFI con structs, arrays, UTF-16, punteros, vtables y bytes.

## 8. Ejemplos SDL2 por FFI

Hay ejemplos mínimos de creación de ventana SDL2 mediante FFI real en `examples/sdl2_ffi/`:

- `linux_sdl2_window_ffi.scml`: carga `libSDL2-2.0.so.0` y llama directamente a `SDL_Init`, `SDL_CreateWindow`, `SDL_Delay`, `SDL_DestroyWindow` y `SDL_QuitSubSystem`.
- `msys2_ucrt64_sdl2_window_ffi.scml`: carga `SDL2.dll` y llama a los mismos símbolos directamente.
- `build_linux.sh` y `build_msys2_ucrt64.sh`: compilan los scripts SCML; no compilan wrappers C porque no existen.

La smoke suite compila los scripts SCML, pero no ejecuta la ventana porque SDL2 y un display gráfico no siempre existen en CI/headless. El ejemplo queda como prueba práctica de FFI dinámico puro: librería cargada una vez, símbolos por nombre, punteros cacheados y cero glue C.

## 9. Game ports SDL2/OpenGL sin glue C

El ejemplo [`examples/sdl2_opengl_hola_game/`](../examples/sdl2_opengl_hola_game/) demuestra un port de juego completo desde `hola.html` a SCML puro:

- `linux_hola_sdl2_opengl_ffi.scml` carga `libSDL2-2.0.so.0` y `libGL.so.1` con `ffi.load`.
- La entrada se limita a teclado mediante `SDL_PumpEvents` + `SDL_GetKeyboardState`; no hay joystick tactil ni mouse por ahora.
- OpenGL se llama directamente con `ffi.call_name` (`glFrustum`, `glRotatef`, `glTranslatef`, `glBegin`, `glVertex3f`, `glFogf`, etc.).
- El estado de coleccionables/rocas vive en arrays nativos creados con `ffi.alloc_array` y se limpia con `ffi.free`.
- Para hacer ports mas robustos, la stdlib FFI expone macros `ffi_call_name3` hasta `ffi_call_name8`, utiles para APIs graficas con muchas coordenadas/flags sin escribir el opcode crudo cada vez.

Compilacion recomendada:

```sh
./examples/sdl2_opengl_hola_game/build_linux.sh
```

Ejecucion en una sesion grafica:

```sh
./bin/scml run examples/sdl2_opengl_hola_game/linux_hola_sdl2_opengl_ffi.scmlbin
```
