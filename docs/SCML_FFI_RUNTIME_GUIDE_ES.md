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

### 5.1 Cargar librerías y declarar funciones

```scml
0B31: "ffi.add_search_path" "examples"
0B31: "ffi.load" "libscml_ffi_native"
0B31: "ffi.declare" "scml_ffi_sum15_i32" "int" "int,int,int,int,int,int,int,int,int,int,int,int,int,int,int"
0B31: "scml_ffi_sum15_i32" 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
03E5: $RETVAL
```

Tipos soportados en firmas y memoria: `bool`, `int8`, `uint8`, `int16`, `uint16`, `int`, `int32`, `uint32`, `int64`, `uint64`, `size`, `float`, `double`, `pointer`, `string`, `void`.

### 5.2 Punteros y memoria

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

### 5.3 Strings C y UTF-16

- `ffi.cstring "texto"` reserva un `char*` terminado en NUL.
- `ffi.read_cstring ptr` lee hasta NUL con límite de seguridad.
- `ffi.write_cstring ptr offset texto max_bytes` escribe texto truncando y garantizando NUL.
- `ffi.utf16 "texto"` reserva UTF-16 little-endian portable para APIs estilo Windows.
- `ffi.read_utf16 ptr [max_units]` convierte UTF-16 a UTF-8 SCML.

### 5.4 Structs, arrays de structs y unions

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

### 5.5 VTables y ABIs

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

- `scml_sdl2_window.c`: librería C pequeña que compila contra SDL2 y exporta `scml_sdl2_open_window_ms`.
- `linux_sdl2_window_ffi.scml`: carga `examples/sdl2_ffi/libscml_sdl2_window.so`, declara la función exportada y abre una ventana en Linux.
- `msys2_ucrt64_sdl2_window_ffi.scml`: carga `examples/sdl2_ffi/scml_sdl2_window.dll`, declara la función exportada y abre una ventana en MSYS2 UCRT64.
- `build_linux.sh` y `build_msys2_ucrt64.sh`: compilan la librería nativa SDL2 y el script SCML correspondiente.

La smoke suite compila los scripts SCML, pero no ejecuta la ventana porque SDL2 y un display gráfico no siempre existen en CI/headless.
