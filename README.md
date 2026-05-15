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

- Labels such as `:MAIN` and `:LOOP`.
- Opcode lines such as `03E5: "hello"`.
- Jump labels through `000A: @LABEL` or `JUMP @LABEL`.
- Header files (`.scmlh`) with `#define`, `#include`, and `macro name(args): ... endmacro`.

## Architecture

- `lexer/` tokenizes individual SCML source lines.
- `parser/` preprocesses headers/macros and parses labels/opcode statements.
- `compiler/` resolves labels and emits compact `.scmlbin` bytecode.
- `opcode/` owns the 200-opcode-capable opcode registry and SCM numeric-code mapping.
- `vm/` loads `.scmlbin` files and runs the fetch/decode/execute loop.
- `stscm/` provides standard-library wrappers as SCML macros instead of adding syntax.
- `compatibilty/` and `compatibility/` contain host-language compatibility stubs.

## Build

```sh
make
```

## Compile and run

```sh
bin/scml compile examples/helloworld.scml examples/helloworld.scmlbin
bin/scml run examples/helloworld.scmlbin --trace
```

## Bytecode

The compiler emits:

1. `SCML` magic.
2. bytecode version.
3. bytecode payload length.
4. variable-length instructions encoded as `opcode:u8`, `argc:u8`, then typed operands.

The VM supports stack values, variables, a fixed heap, an in-memory entity table, tracing, and error propagation through C API error buffers.
