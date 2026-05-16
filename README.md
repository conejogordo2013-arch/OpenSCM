# OpenSCM SCML

SCML (Scripting Control Markup Language) is a compact, SCM-looking embedded scripting language and virtual machine implemented in C. It keeps GTA San Andreas SCM-style visual structure while providing a modern embeddable execution core.

## Language shape

SCML source deliberately avoids Lua/Python-like syntax:

```scml
:MAIN
0004: counter 0
:LOOP
03E5: counter
0006: counter counter 1
00D8: 3 counter @LOOP
0001:
```

Supported source concepts include:

- Labels such as `:MAIN`, `:LOOP`, and `:FUNC_HEAL_PLAYER`.
- Opcode lines such as `03E5: "hello"`.
- Jump labels through `000A: @LABEL` or `JUMP @LABEL`.
- SCM-style calls through `CALL @FUNC` and `return`.
- Thread/event handler termination through `end_thread`.
- Header files (`.scmlh`) with `#define`, `#include`, and `macro name(args): ... endmacro`.
- Optional SCM-style flow helper libraries such as `stscm/scm_flow.scmlh`, which provide `goto`, `gosub`, `if_eq`, `goto_if_false`, and `END_SCRIPT` as macros over existing opcodes.
- A general-purpose `std.scmlh` include that aggregates the standard wrappers, SCM-style flow helpers, and core opcode aliases without adding any GTA-specific behavior.
- Optional convenience assignment such as `$PLAYER_HEALTH += 50`, compiled to the existing `ADD` opcode.

## Architecture

- `lexer/` tokenizes individual SCML source lines.
- `parser/` preprocesses headers/macros and parses labels/opcode statements.
- `compiler/` resolves labels to addresses and emits compact `.scmlbin` bytecode.
- `opcode/` owns the 200-opcode-capable opcode registry and SCM numeric-code mapping.
- `vm/` loads `.scmlbin` files and runs the fetch/decode/execute loop.
- `stscm/` provides standard-library wrappers as SCML macros instead of adding high-level syntax.
- `compatibilty/` and `compatibility/` contain host-language compatibility stubs.

## Runtime features

- Global event registry with multiple label handlers per event.
- Event queue dispatch from SCML (`0A31:` / `EVENT_TRIGGER`) or host API (`scml_vm_trigger_event`).
- Call stack with `CALL` and `return`; non-`$` variables inside calls are local to the active frame.
- Garbage-safe heap object references through integer handles instead of raw pointers.
- Array/block memory opcodes: `0B10 ALLOC`, `0B11 FREE`, `0B12 READ`, `0B13 WRITE`, `0B14 ARRAY_CREATE`, plus real dynamic array helpers `0B15 ARRAY_PUSH`, `0B16 ARRAY_POP`, and `0B17 ARRAY_LEN`.
- Typed signed integer casts/constructors through `INT`, `INT2`, `INT4`, `INT8`, `INT16`, `INT32`, and `INT64` (with lowercase aliases such as `int32`/`int64`); literals outside 32-bit range are encoded as 64-bit operands.
- Advanced runtime types (`bool`, `null`, `float32`, `string`, `object`) with `TYPE_OF`, `IS_TYPE`, and casts/construction macros.
- Class/object runtime with class definitions, inheritance, object creation, fields, instance checks, and label-backed method dispatch (`CLASS_DEFINE`, `CLASS_EXTENDS`, `OBJECT_NEW`, `FIELD_*`, `CLASS_METHOD`, `METHOD_CALL*`).
- Debug tracing with source line mapping, `scml_vm_step`, and a memory inspector (`scml_vm_dump_memory`).
- Multi-script compilation for shared global symbols and cross-script calls.

## Build

```sh
make
```

## Compile and run

```sh
bin/scml compile examples/helloworld.scml examples/helloworld.scmlbin
bin/scml run examples/helloworld.scmlbin --trace
```

Compile several scripts into one global bytecode image:

```sh
bin/scml compile examples/modular_main.scml examples/modular_functions.scml examples/modular.scmlbin
bin/scml run examples/modular.scmlbin
```

Inspect memory and trace source lines:

```sh
bin/scml run examples/debugging.scmlbin --trace --dump-memory
```

Compile and run a dynamic-array example:

```sh
bin/scml compile examples/dynamic_arrays.scml examples/dynamic_arrays.scmlbin
bin/scml run examples/dynamic_arrays.scmlbin --dump-memory
```

Compile and run typed integer examples:

```sh
bin/scml compile examples/int_types.scml examples/int_types.scmlbin
bin/scml run examples/int_types.scmlbin
```

Compile and run classes / advanced types:

```sh
bin/scml compile examples/classes_types.scml examples/classes_types.scmlbin
bin/scml run examples/classes_types.scmlbin --dump-memory
```

Compile the exhaustive opcode/library showcase:

```sh
bin/scml compile examples/all_opcodes_showcase.scml /tmp/all_opcodes_showcase.scmlbin
bin/scml run /tmp/all_opcodes_showcase.scmlbin --dump-memory
```




## SCML estándar avanzado (`std.scmlh`)

La librería estándar ahora incluye utilidades más robustas para construir runtimes/hosts más flexibles:

- Modelo de estado y errores (`SCML_OK`, `SCML_ERR_*`) para flujos de ejecución tolerantes a fallos.
- Macros de validación (`ASSERT_*`, `REQUIRE_TRUE`, `RETURN_IF_ERR`) para control de errores más consistente.
- Acceso seguro de arreglos (`ARRAY_SAFE_GET`) con reporte de error explícito.
- Metadatos de empaquetado estilo `.scmr` (tipo contenedor `.jar`) mediante `PACKAGE_*` y objetivos de artefacto (`TARGET_EXE`, `TARGET_DLL`, `TARGET_SO`, `TARGET_SCMLVM_LIB`, `TARGET_SCMLSTD_LIB`, `TARGET_SCMLABI_LIB`).

Esto permite describir, desde SCML, builds orientados a ejecutables, bibliotecas dinámicas y paquetes con recursos binarios/aplicación. Un host o toolchain puede leer estas variables globales para generar `.exe`, `.dll`, `.so` y contenedores `.scmr`.

Ejemplo:

```sh
bin/scml compile examples/scmr_package.scml examples/scmr_package.scmlbin
bin/scml run examples/scmr_package.scmlbin
```

Empaquetar en contenedor `.scmr` (aplicación + assets binarios):

```sh
bin/scml pack examples/scmr_package.scmlbin examples/scmr_package.scmr examples/scmr_asset.txt
```

Formato SCMR (v1): cabecera (`SCMR`), tabla de entradas (`app.scmlbin` + assets), y payload binario concatenado. Esto permite un flujo tipo `.jar` para distribución de aplicaciones SCML con recursos.

## GTA/CLEO compatibility example

`examples/free_camera_vehicle.scml` is a SCML-compatible conversion of a CLEO-style vehicle free-camera script.  The companion header `examples/gta_camera_compat.scmlh` maps GTA-specific memory, input, vehicle, and camera operations to native `CALL`s so the source compiles with the core SCML compiler while an embedding host provides the game-specific implementations.

Compile it with:

```sh
bin/scml compile examples/free_camera_vehicle.scml examples/free_camera_vehicle.scmlbin
```

## C++ embedding

SCML ships a small C++ wrapper in `cpp/scml_vm.hpp` for game and simulation hosts:

```cpp
SCML_VM vm;
vm.init();
vm.loadScript("examples/gameplay.scmlbin");
vm.registerFunction("SpawnEnemy", SpawnEnemy, &world);
vm.registerFunction("Log", Log);
vm.triggerEvent("ON_START");
vm.run();

while (running) {
    vm.triggerEvent("ON_TICK");
    vm.update();
}
```

Native functions use the C ABI type `ScmlNativeFunc` and receive typed `ScmlValue` arguments (`int`, `float`, or `string`). Return values are written to `$RETVAL`. The gameplay example (`examples/gameplay_embed.cpp`) demonstrates a C++ `Entity` vector, native `SpawnEnemy`, `SpawnMedkit`, and `Log` functions, and a script-driven update loop.

Build and run it with:

```sh
make cpp-example
```

## Bytecode

The compiler emits:

1. `SCML` magic.
2. bytecode version.
3. bytecode payload length.
4. source line table entry count.
5. variable-length instructions encoded as `opcode:u8`, `argc:u8`, then typed operands.
6. `(pc, source_line)` entries for error reporting and trace output.

The VM supports stack values, variables, call frames, a safe reference heap, an in-memory entity table, event dispatch, tracing, and error propagation through C API error buffers.

## Debugger, hot reload, visual editor, and graphics

The ecosystem now includes the first modular tools around the VM:

- `debugger/` provides `ScmlDebugger`, breakpoints by bytecode PC or label, `step`, `continue`, and state/memory dumps backed by VM line mapping.
- `hotreload/scml_hot_reload.hpp` watches a `.scmlbin` file, reloads it when the timestamp changes, clears stale event bindings, and preserves existing VM globals/native registrations where possible.
- `runtime/game_runtime.hpp` is a small game-runtime layer with a C++ entity list and native functions that SCML can call from gameplay scripts.
- `graphics/scml_graphics.hpp` provides an SDL2/OpenGL renderer when headers are available and a headless fallback otherwise.
- `editor/scml_editor.cpp` is a visual-editor prototype shell with compile/run/debug wiring, output console text, debugger integration, and optional graphics initialization.

Build the editor prototype with:

```sh
make editor-example
```

Hot reload is intended to be polled inside the same real-time loop as events:

```cpp
SCML_HotReload reload;
reload.watch(&vm, "examples/gameplay.scmlbin");
while (running) {
    reload.poll();
    vm.triggerEvent("ON_TICK");
    vm.update();
}
```


## LWSCMGL (idea tipo LWJGL para SCML)

Se añadió una base inicial de **LWSCMGL** para integración gráfica lightweight desde hosts C++:

- `runtime/lwscmgl.hpp` define un contexto mínimo y binding nativo `LWSCMGL_DrawPoint`.
- `examples/lwscmgl_demo.scml` llama a la API desde SCML.
- `examples/lwscmgl_demo.cpp` registra los natives y ejecuta el script.

Ejecutar demo:

```sh
make lwscmgl-example
```
