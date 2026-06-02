# OpenSCM SCML

SCML (Scripting Control Markup Language) is a compact, SCM-looking embedded scripting language and virtual machine implemented in C. It keeps GTA San Andreas SCM-style visual structure while providing a modern embeddable execution core.


## Curso completo en español

Se añadió una guía didáctica completa para aprender SCML desde cero hasta uso avanzado, incluyendo sintaxis, opcodes, macros, memoria, eventos, async, terminal rendering, integración nativa y una valoración práctica del lenguaje:

- [`docs/SCML_COMPLETE_COURSE_ES.md`](docs/SCML_COMPLETE_COURSE_ES.md)
- [`docs/SCML_FFI_RUNTIME_GUIDE_ES.md`](docs/SCML_FFI_RUNTIME_GUIDE_ES.md)

## Language shape

SCML keeps its legacy SCM/opcode syntax as the stable core, and now also accepts an opt-in modern C/C#/SCML surface that is lowered by the preprocessor into the same legacy bytecode path. Legacy source remains valid:

```scml
:MAIN
0004: counter 0
:LOOP
03E5: counter
0006: counter counter 1
00D8: 3 counter @LOOP
0001:
```

Modern surface syntax can live in the same `.scml` files:

```scml
use "std.scmlh";

script MAIN {
    let $count: u32 = 0;
    while ($count < 3) {
        $count += 1;
    }

    for (let $i: i32 = 0; $i < 8; $i++) {
        if ($i == 2) {
            continue;
        }
        if ($i == 6) {
            break;
        }
        $count += $i;
    }

    if ($count > 3) {
        print("modern SCML");
    } else {
        log("unexpected count");
    }
}

fn HELPER() {
    print("called through legacy CALL");
}
```

Supported source concepts include:

- Labels such as `:MAIN`, `:LOOP`, and `:FUNC_HEAL_PLAYER`.
- Opcode lines such as `03E5: "hello"`.
- Jump labels through `000A: @LABEL` or `JUMP @LABEL`.
- SCM-style calls through `CALL @FUNC` and `return`.
- Thread/event handler termination through `end_thread`.
- Header files (`.scmlh`) with `#define`, legacy `#include`, modern `use`, legacy `macro name(args): ... endmacro`, and modern `macro name(args) { ... }`.
- Optional convenience assignments such as `$PLAYER_HEALTH += 50`, `$MASK &= 255`, and `$FLAGS >>= 1`, compiled to the matching arithmetic/bitwise opcodes.
- Modern declarations such as `let $COUNT: u32 = 32;`, `var`, `const`, uppercase `AUTO`, C-style typed declarations (`INT y = 32;`, `SCML::VECTOR<INT> values;`), and simple arithmetic initializers lowered to existing `TYPE_DECL`, `SET`, and arithmetic opcodes.
- Modern blocks: `script`, `fn`/`function`, `task`, `if/else if/else`, `while`, C-style `for(init; condition; post)`, `break`, `continue`, `goto`, `call`, `spawn`, `return`, `halt`, and `yield`.
- Advanced surface-only metadata blocks for `CLASS`, `INTERFACE`, `NAMESPACE`, `MODULE`, `TMPL`, `concept`, `enum class`, access sections (`PUBLIC:`, `PRIVATE:`, `PROTECTED:`), virtual/inline member declarations, lightweight exceptions (`try`/`catch`/`finally` blocks plus `throw` state), lambda stubs, and structured bindings such as `AUTO [x, y] = point;`.
- Modern calls such as `print(value);`, `log(value);`, `wait(ms);`, and native module calls like `runtime.get_ticks() -> $TICKS;`.
- Modern package imports through `use "std.scmlh";`, `use <mathkit.scmlh>;`, or namespace-style `use vendor.mathkit;`.
- The global STD surface in `std.scmlh` / `stscm/std.scmlh` is frozen for compatibility. New code should import domain modules from `stscm/modules/` (`io`, `collections`, `types`, `fs`, `concurrency`, `ranges`, `data`, `ffi`, `vm`, `meta`) and use lower_snake_case helpers without `SCML_*` prefixes. Legacy `SCML_*` helpers live in `stscm/compat/legacy_scml_prefix.scmlh` and are deprecated. See [`docs/SCML_STD_MODULES_ES.md`](docs/SCML_STD_MODULES_ES.md).
- C++17 superset pack: `[[...]]` attributes, inline/constexpr/static declarations, executable structured bindings for SCML aggregates, C++17-style `if (init; condition)`, literal `if constexpr` branch erasure, `fold_add`/`fold_mul`/`fold_any`/`fold_all` lowering, host-backed filesystem exists, and optional/variant/any accessors. See [`docs/SCML_CPP17_SUPERSET_ES.md`](docs/SCML_CPP17_SUPERSET_ES.md) and [`examples/cpp17_superiority.scml`](examples/cpp17_superiority.scml).
- C++20 domination pack: executable range-for over SCML ranges, ranges/views helpers, `requires(value, type) -> out` concepts contracts, `co_await`/`co_return` coroutine lowering, `consteval_*` arithmetic helpers, and `import module.name;` metadata. See [`docs/SCML_CPP20_DOMINATION_PACK_ES.md`](docs/SCML_CPP20_DOMINATION_PACK_ES.md) and [`examples/cpp20_domination.scml`](examples/cpp20_domination.scml).

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
- Hostless application helpers through the default runtime module registry for files, console, timing, input, network, audio, windows, graphics when available, data transformations (`data.hash32`, URL encode/decode, split, flat `json_get`), environment variables (`env.get`/`env.set`), and advanced FFI memory/pointer helpers (`ffi.read_bytes`, `ffi.write_bytes`, `ffi.memcmp`, `ffi.memmove`, `ffi.is_null`).
- Cooperative async tasks with `ASYNC_SPAWN`, `ASYNC_DONE`, and polling/await macros on top of the VM event queue.
- Compile-time checked `TYPE_DECL` / `LET_I32` / `LET_F32` / `LET_STR` declarations plus runtime `TYPE_ASSERT` for advanced static-typing style contracts.

## Build

OpenSCM can be built with the repository `Makefile` on POSIX-like shells, MSYS2/MinGW, and other GNU Make environments:

```sh
make
make bin/scml_gameplay_example bin/scml_editor
```

For IDEs and platforms where Make variables or shell utilities differ, use the portable CMake project instead. CMake auto-detects installed optional runtime backends such as SDL2, OpenGL, Vulkan, Direct3D, and Metal when available, and only registers compiled-in optional backends at runtime:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Disable optional backend auto-detection with `-DSCML_AUTODETECT_BACKENDS=OFF` when a fully headless build is desired.

## Project-scale workflow

SCML now supports manifest-driven projects for large codebases. A `scml.pkg` file can list many source files, package include roots, an output artifact, and a documented `jobs` setting for build orchestration metadata:

```ini
name = "enterprise-demo"
source_dir = "src"
package_dir = "packages/mathkit"
define = "PROJECT_MODE enterprise"
output = "build/enterprise-demo.scmlbin"
jobs = 4
```

Useful tooling commands:

```sh
bin/scml init my_app my_app
bin/scml build my_app/scml.pkg
bin/scml check my_app/scml.pkg
bin/scml metadata my_app/scml.pkg
bin/scml fmt my_app/src/main.scml
```

Project manifests can name individual files with `source =`, recursively discover huge source trees with `source_dir =`, expose package roots with `package`/`package_dir`, and publish build-time feature switches with `define =`. `bin/scml metadata` prints the resolved source graph, package roots, defines, output, and job count for editor/CI integrations. Project packages are added to `SCML_PATH`, so headers can be included ergonomically with `#include <package_header.scmlh>` or modern `use <package_header.scmlh>;`. The standard library also exposes richer type-contract aliases (`bool`, `u32`, `i64`, `f64`, `ref`, `array<T>`, `vec<T>`, `option<T>`, `result<T>`) on top of the current VM value families, host-backed OS-thread helpers (`THREAD_CREATE_SLEEP`, `THREAD_DONE`, `THREAD_JOIN`, `THREAD_YIELD`) for real native concurrency around runtime jobs, and raised VM capacity defaults for much larger scripts, event queues, async tasks, globals, stack frames, native modules, and heap objects.

The migration is intentionally incremental: `std.scmlh` now uses modern brace-style macro declarations in the migrated std/example headers and includes lower-case modern aliases such as `set`, `add`, `array_new`, `array_get`, `array_set`, `vector_new`, `vector_push`, `vector_pop`, `type_assert`, `async_spawn`, and `thread_join`, while the uppercase legacy macros continue to work. The starter examples (`helloworld`, `variables`, `functions`, `complexlogic`, `std_usage`, `dynamic_arrays`, `hostless_async_static`, and the enterprise project) now demonstrate mixed modern + legacy SCML style.

Try the package/project example:

```sh
bin/scml build examples/enterprise_project/scml.pkg
bin/scml run examples/enterprise_project/build/enterprise-demo.scmlbin
```

## Compile and run

```sh
bin/scml compile examples/helloworld.scml examples/helloworld.scmlbin
bin/scml run examples/helloworld.scmlbin --trace
```

Compile and run the optimized 60 FPS ASCII cube demo. The script keeps the frame small and batches complete rows, while the VM buffers `PRINT_RAW` until a frame boundary (`WAIT`) so MSYS2/Windows terminals do not stall on per-character output:

```sh
bin/scml compile examples/rotating_ascii_cube_60fps.scml examples/rotating_ascii_cube_60fps.scmlbin
bin/scml run examples/rotating_ascii_cube_60fps.scmlbin
```

Compile several scripts into one global bytecode image:

```sh
bin/scml compile examples/modular_main.scml examples/modular_functions.scml examples/modular.scmlbin
bin/scml run examples/modular.scmlbin
```

Compile and run the mixed modern/legacy syntax demo:

```sh
bin/scml compile examples/modern_surface.scml examples/modern_surface.scmlbin
bin/scml run examples/modern_surface.scmlbin
```

Compile and run the hostless async/static-typing demo:

```sh
bin/scml compile examples/hostless_async_static.scml examples/hostless_async_static.scmlbin
bin/scml run examples/hostless_async_static.scmlbin
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

## Terminal rendering and complex math/string helpers

SCML can now drive terminal animation directly from scripts with raw printing, ANSI clearing, trigonometric/math opcodes, string concatenation, and string repetition. The examples below intentionally avoid `#include`, std macros, and native generated-frame helpers so they demonstrate the language/VM surface itself:

```scml
:MAIN
0B33:
0004: $TICK 0
:FRAME
0B32: "\033[H"
; ...SCML loop math decides what each terminal cell prints...
0006: $TICK $TICK 1
000B: 16
000A: @FRAME
```

Useful raw opcodes include `PRINT_RAW` (`0B32`), `CONSOLE_CLEAR` (`0B33`), `SIN` (`0B34`), `COS` (`0B35`), `TAN` (`0B36`), `SQRT` (`0B37`), `ATAN2` (`0B38`), `FLOOR` (`0B39`), `CEIL` (`0B3A`), `ROUND` (`0B3B`), `ABS` (`0B3C`), `STR_REPEAT` (`0B3D`), `CONSOLE_COLOR` (`0B3E`), `CONSOLE_RESET` (`0B3F`), `CONSOLE_MOVE` (`0B40`), `CONSOLE_ERASE_LINE` (`0B41`), and `CONSOLE_STYLE` (`0B42`). Terminal control opcodes validate ranges before emitting ANSI sequences (`0..255` colors, `1..9999` cursor coordinates, and `0..9` style codes). For high-frequency ASCII rendering, use the safe flat-buffer intrinsics: `SPAN_CREATE` (`0B43`) allocates a contiguous byte span, `SPAN_PIN` (`0B44`) marks it immovable for native/console handoff, `SPAN_FILL` (`0B45`) bulk-clears a span or int array with bounds checks, `SPAN_WRITE_U8`/`SPAN_READ_U8` (`0B46`/`0B47`) mutate byte cells safely, and `CONSOLE_RENDER_SPAN` (`0B48`) renders a complete pinned framebuffer with one VM intrinsic instead of thousands of `PRINT_RAW` calls.

Pure SCML demos:

```sh
bin/scml compile examples/loading_spinner.scml examples/loading_spinner.scmlbin
bin/scml run examples/loading_spinner.scmlbin

bin/scml compile examples/terminal_render_math.scml examples/terminal_render_math.scmlbin
bin/scml run examples/terminal_render_math.scmlbin

bin/scml compile examples/rotating_ascii_cubes.scml examples/rotating_ascii_cubes.scmlbin
bin/scml run examples/rotating_ascii_cubes.scmlbin
```


## Native module boundary (portable VM core)

The VM core is now designed to delegate host/platform behavior via native modules instead of performing it directly.

- New opcode: `CALL_NATIVE` (`0B31`) with qualified target names like `module.function`.
- VM API adds dynamic module registration and resolution:
  - `scml_vm_register_module(name, resolver)`
  - `scml_vm_call_native("module.function", args...)`
  - runtime registry also supports `unregister_module(name)` semantics via `scml_runtime_unregister_module`
- Legacy opcodes that imply platform behavior (`WAIT`, `FILE_READ`, `FILE_WRITE`, `INPUT`, `ENTITY_SPAWN`) now route through module callbacks (`runtime.*`, `file.*`, `input.*`, `gpu.*`) and do not execute host APIs in VM core.

Example targets:
- `gpu.drawTriangle`
- `audio.play`
- `file.read`
- `net.request`



## Modular standard library runtime (JVM-style boundary)

SCML now ships a runtime-side modular standard-library registry outside VM core:

- Runtime API: `register_module(name, function_table)`, `resolve_module(name)`, `call_module(module.function, args...)`.
- Reference modules: `gpu`, `audio`, `file`, `net`, `input` (plus `runtime.wait`).
- VM remains bytecode/memory/call-dispatch only; host features are plugin modules.
- Backends are replaceable (e.g., SDL/OpenGL/Vulkan/sockets) without changing VM bytecode.

The default CLI installs a builtin module registry with null backends as placeholders. Production hosts should register real backend function tables from C/C++.


### Multi-backend native modules

`runtime/scml_runtime_modules.*` now supports per-module **multiple backends** with runtime selection:

- `register_module(name, function_table)` (compat/default backend)
- `register_backend(module, backend_impl)`
- `resolve_module(name)`
- `call_module(module.function, args...)` through VM `CALL_NATIVE`
- `select_backend(module, backend_name)`

Builtin abstract modules and function families:

- `gpu`: `create_window`, `begin_frame`, `draw_triangle`, `draw_mesh`, `present` (+ `load_texture`)
- `audio`: `play_sound`, `stop_sound`, `set_volume`, `stream_audio`
- `input`: `get_keyboard_state`, `get_mouse_position`, `poll_events`
- `file`: `open_file`, `read_file`, `write_file`, `list_directory`
- `net`: `open_socket`, `send_data`, `receive_data`

Builtin backend names are pre-registered as placeholders (null backend callbacks) for portability: 
`gpu`: `opengl`, `vulkan`, `directx12`, `metal`, `opengles`; `audio`: `sdl_audio`, `openal`; others default-only by design.

This keeps VM core OS/hardware agnostic while allowing host runtime backends to be swapped without bytecode changes.


Use `--no-builtin-modules` to run with **zero optional capabilities** installed; in this mode `CALL_NATIVE` fails in a controlled way for missing modules.


### Extensible instruction style (without touching VM C for every new instruction)

The parser now supports **native-instruction style**: if an opcode token is unknown but looks like `module.function`, it is compiled as `CALL_NATIVE "module.function" ...args`.

That means you can add new host/runtime capabilities by installing modules/backends, then call them directly from SCML without adding a new C opcode each time:

```scml
gpu.draw_triangle 0 0 1 0 0 1
audio.play_sound "explosion.wav"
file.open_file "data.txt"
gpu.draw_triangle: 0 0 1 0 0 1  ; SCM-style colon also works
```

This keeps the VM core stable while enabling complex SCML libraries layered over module APIs.


SCML-style uppercase wrappers are also available in `stscm/std.scmlh` (`DRAW_TRIANGLE`, `PLAY_SOUND`, `OPEN_FILE`, `READ_FILE`, `CONNECT_SOCKET`) to keep a classic SCML visual style while still using `CALL_NATIVE`.


Default runtime module catalog now also includes optional `image.*` and extended `file.*` TXT/image-oriented entries (all placeholder backends by default). They are intended to be consumed through SCML libraries/macros over `CALL_NATIVE`, keeping VM/core unchanged while API surface expands.


`runtime/scml_runtime_modules.c` now includes optional real backend imports behind compile flags (`SCML_USE_SDL2`, `SCML_USE_OPENGL`, `SCML_USE_OPENGLES`) and implements real TXT read/write runtime handlers by default (`file.read_txt`, `file.write_txt`).


The runtime module layer now declares broader backend hooks (Vulkan, DirectX, Metal, SDL2, OpenGL, OpenGL ES) behind compile flags and exposes `*.backend_info` endpoints so SCML libraries can introspect active compile-time backend capabilities via `CALL_NATIVE`.

For practical per-OS installation/build requirements (MSYS2/Linux/macOS, X11, Vulkan, SDL2, etc.), see `README_RUNTIME_BACKENDS.md`.


Runtime defaults now include additional concrete handlers (`runtime.wait` sleep behavior and baseline `net.open_socket`/`net.send_data` responses) so SCML libraries can prototype richer systems with less boilerplate while still using the native-module boundary.

## Generic dynamic FFI

SCML includes a generic Foreign Function Interface for loading host dynamic libraries
without teaching the VM about any specific external API. The core loader exposes:

- `scml_ffi_load_library(path)` / `CALL_NATIVE "load_library" path`
- `scml_ffi_unload_library(handle)` / `CALL_NATIVE "unload_library" handle`
- `scml_ffi_get_symbol(handle, function_name)` for explicit symbol lookup
- `CALL_NATIVE "FunctionName" ...args` for automatic symbol search across every
  loaded library and invocation through the generic call layer

Arguments are converted from VM values to native ABI values generically. By
default the VM maps `int` to `int32_t`, `float` to `float`, strings to
`const char *`, and pointer values to `void *`. Return values default to `int`;
scripts can set `$FFI_RETURN_TYPE` to `"int"`, `"i64"`, `"float"`,
`"double"`, `"pointer"`, `"string"`, or `"void"` before a call when the native
function returns another type. Scripts can also set `$FFI_ARG_TYPES` to a
comma-separated type list such as `"int,double,pointer"` to force exact ABI
argument types for large native libraries with mixed signatures. For repeated
calls, `CALL_NATIVE "ffi.declare" "FunctionName" "return_type" "arg,arg"` stores
a reusable signature in the FFI registry, so later `CALL_NATIVE "FunctionName"`
calls do not need to keep resetting global type variables.

The implementation uses `LoadLibrary` / `GetProcAddress` on Windows and
`dlopen` / `dlsym` on Unix-like systems. The library, symbol, signature, and
search-path registries grow dynamically instead of using fixed slots, duplicate
library loads are reference-counted, resolved symbols are cached, and unresolved
symbols are reported as controlled VM runtime errors. `CALL_NATIVE "ffi.add_search_path" path`
registers lookup directories, and `CALL_NATIVE "load_library" path flags`
optionally accepts bit flags (`1` now, `2` lazy, `4` local, `8` global) so large
libraries with transitive dependencies can be loaded with `RTLD_GLOBAL`-style
visibility when the platform supports it. Extension probing lets scripts load a
basename such as `"libexample"` from registered search paths and let the host pick
`.so`, `.dylib`, or `.dll`. For struct/buffer-heavy native APIs, the generic
allocator helpers (`ffi.alloc`, `ffi.cstring`, `ffi.utf16`, `ffi.free`,
`ffi.ptr_add`, `ffi.peek_ptr`, `ffi.poke_ptr`, `ffi.read`, `ffi.write`,
`ffi.memcpy`, `ffi.memset`, `ffi.read_cstring`, and `ffi.read_utf16`) let scripts
build native memory layouts and pass pointers without adding API-specific modules
to the VM core. `ffi.call_ptr` / `ffi.call_abi_ptr` invoke function pointers
returned by loaders or native APIs, while `ffi.vtable_call` / `ffi.com_call` read
COM-style vtables, prepend the object pointer, and dispatch a method by slot.
`ffi.union_begin` defines overlapped native layouts for APIs that use C unions.
For diagnostics and visibility, scripts can call `ffi.capabilities` / `ffi.info`,
`ffi.abi_supported`, `ffi.last_error`, and `ffi.clear_error` to inspect the active
FFI feature set and the latest controlled native interop error.

Example native library and script files are provided in `examples/ffi_native.c`
and `examples/ffi_dynamic_call.scml`:

```sh
cc -shared -fPIC -o examples/libscml_ffi_native.so examples/ffi_native.c
bin/scml compile examples/ffi_dynamic_call.scml /tmp/ffi_dynamic_call.scmlbin
bin/scml run /tmp/ffi_dynamic_call.scmlbin
```

The FFI layer is intentionally API-agnostic: it does not embed SDL, OpenGL,
Vulkan, audio, filesystem modules, or any other external library-specific logic.
