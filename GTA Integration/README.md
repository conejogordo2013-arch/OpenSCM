# GTA Integration

This directory keeps the GTA SA/CLEO work **outside** the SCML language core.
SCML remains a general-purpose embedded scripting language; GTA behavior is
provided by headers and a host/plugin layer.

## Layers

1. `scmlh/` - SCML compatibility headers such as `gtasa.scmlh`, actor,
   vehicle, camera, memory, text, and streaming wrappers.
2. `host/` - a C++ host/native registry. The included desktop host is a
   deterministic mock that proves `.scmlbin` files can call GTA-style natives.
3. `cleo/` - a CLEO/ASI plugin skeleton that shows where a real GTA loader
   should load `.scmlbin`, register natives, and tick the VM each frame.
4. `examples/` - SCML gameplay scripts that compile with the normal OpenSCM
   compiler and call only host-provided GTA natives.
5. `sanny/` - a Sanny Builder external-tool profile sketch for editing `.scml`
   and compiling/deploying `.scmlbin` without modifying the language.

## Workflow

```sh
bin/scml compile "GTA Integration/examples/spawn_demo.scml" /tmp/spawn_demo.scmlbin
c++ -std=c++17 -I. -o /tmp/gtasa_desktop_host \
  "GTA Integration/host/gtasa_desktop_host.cpp" \
  vm/vm.o compiler/compiler.o parser/parser.o lexer/lexer.o opcode/opcode.o
/tmp/gtasa_desktop_host /tmp/spawn_demo.scmlbin
```

For GTA SA, replace the desktop mock natives in `host/gtasa_host.hpp` with real
CLEO/ASI calls and wire the `cleo/scml_cleo_plugin.cpp` skeleton to your plugin
SDK callbacks.

## Design rules

- Do not add GTA opcodes to the SCML core language.
- Put user-facing commands in `.scmlh` macros.
- Put game-engine behavior in native host functions registered with the VM.
- Prefer typed natives such as `GTA_CreateCar` over a single generic dispatcher
  once a command is implemented for real.
- Keep `main_compat.scmlh` as a legacy bridge for large decompiled `main.txt`
  ingestion; new scripts should include `scmlh/gtasa.scmlh`.
