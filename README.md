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
- Array/block memory opcodes: `0B10 ALLOC`, `0B11 FREE`, `0B12 READ`, `0B13 WRITE`, and `0B14 ARRAY_CREATE`.
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


## Mega advanced script (ASCII + matemáticas + OOP/RTTI + namespaces + preprocesado + STL)

Si quieres un ejemplo "todo en uno" orientado a superficie ISO completa, usa:

```sh
bin/scml compile examples/mega_everything_advanced.scml examples/mega_everything_advanced.scmlbin
bin/scml run examples/mega_everything_advanced.scmlbin --trace
```

El archivo `examples/mega_everything_advanced.scml` combina render ASCII complejo, punteros, matemáticas avanzadas, RTTI/OOP orientado a namespaces y capa STL/template-style mediante macros SCML.

## SCML Mega Bootstrap Surface (SCML-style)

The repository now includes a dedicated SCML bootstrap catalog under `scmlspec/` with:

- `scml_iso_bootstrap.scmlh`: SCML preprocessor/type/semantic feature flags.
- `scml_keyword_inventory.scml`: SCML-script inventory for full reserved keyword surface.
- `scml_mega_bootstrap_manifest.md`: domain-by-domain bootstrap manifest.

This is intentionally SCML-oriented and avoids requiring C++ syntax in SCML user scripts.


## Advanced runtime extensions (input, conversion, string concat, extra conditionals)

SCML now includes additional opcodes and std macros for richer scripting:

- `INPUT` (`0B21`) for stdin capture
- `STRCAT` (`0B22`) for string concatenation
- `TO_INT` (`0B23`) and `TO_FLOAT` (`0B24`) for type conversion
- `MOD` (`0B20`) arithmetic remainder
- `IF_GE` (`0B25`) and `IF_LE` (`0B26`) for extended conditionals
- `BIT_AND/OR/XOR/NOT` (`0B27`-`0B2A`) and `SHL/SHR` (`0B2B`/`0B2C`) for bitwise workflows
- `POW` (`0B2D`) for exponent math
- `STRLEN` (`0B2E`) + `SUBSTR` (`0B2F`) for richer string handling
- `ARRAY_LEN` (`0B30`) for heap/array introspection
- Added float family aliases in std macros: `FLOAT`, `FLOAT2`, `FLOAT4`, `FLOAT8`, `FLOAT16`, `FLOAT32`, `FLOAT64`

Demo:

```sh
bin/scml compile examples/mega_input_advanced.scml examples/mega_input_advanced.scmlbin
printf '33\n' | bin/scml run examples/mega_input_advanced.scmlbin
```
