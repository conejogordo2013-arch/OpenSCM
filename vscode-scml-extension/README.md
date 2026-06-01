# SCML Advanced Tools (VS Code)

A mature IDE layer for SCML/SCMLH projects. The extension provides LSP-style language intelligence without requiring a separate server process, and it is designed to work with the modern SCML compiler surface (`script`, `fn`, `let`, `for`, `else if`, `break`, `continue`, FFI wrappers, and legacy opcode syntax in the same project).

## Language intelligence

- Syntax highlighting for labels, directives, strings, numbers, modern keywords, compound operators, and numeric opcodes.
- IntelliSense completions for numeric opcodes, opcode names, standard macros, labels, directives, modern type names, and snippets.
- Hover documentation for opcodes, macros, labels, and SCML static type names.
- Go to Definition for labels, macros, defines, and variables.
- Find References and Rename Symbol for label-like SCML symbols.
- Document symbols and folding ranges for labels and macro blocks.
- Document formatting for SCML-style indentation and argument spacing.
- Quick Fix to create missing labels from unresolved `@LABEL` diagnostics.
- Problems diagnostics for duplicate labels, unresolved labels, missing includes, unknown opcodes/macros, and compiler errors on save.

## Build/run workflow

Commands available from the Command Palette, editor title, and editor context menu:

- `SCML: Select CLI Executable` — choose the exact `scml`/`scml.exe` binary to use. This is the recommended way to point VS Code at a compiler build that supports the modern syntax.
- `SCML: Show Resolved CLI Executable` — display the binary path that will be used.
- `SCML: Compile Current File` — compile the active `.scml` source to `.scmlbin`.
- `SCML: Run Current Binary` — run the expected `.scmlbin` for the active source.
- `SCML: Compile and Run Current File` — compile and immediately run.
- `SCML: Check Current File` — call `scml check` for the active source/header.
- `SCML: Build Project Manifest` — build the configured `scml.pkg`.
- `SCML: Show Project Metadata` — run `scml metadata` for the configured manifest.
- `SCML: Restart Language Services` — refresh opcode/macro catalogs and diagnostics.

The extension resolves the SCML executable from `scml.executablePath`, workspace `bin/scml`, workspace `scml`, or `PATH`.

## Settings

- `scml.executablePath`: path to the SCML CLI. Supports `${workspaceFolder}` and can be selected with `SCML: Select CLI Executable`.
- `scml.defaultBinOutputDir`: output directory for `.scmlbin` files. Supports `${workspaceFolder}`, `${fileDirname}`, `${fileBasename}`, and `${fileBasenameNoExtension}`.
- `scml.additionalCompileArgs`: extra arguments appended to `scml compile` before the input/output paths.
- `scml.additionalRunArgs`: extra arguments appended to `scml run` after the binary path.
- `scml.runWithTrace`: include `--trace` when running binaries.
- `scml.runInTerminal`: run binaries in an integrated terminal instead of the Output panel.
- `scml.manifestPath`: default project manifest used by build/metadata commands. Supports `${workspaceFolder}`.
- `scml.clearOutputBeforeTask`: clear the SCML output channel before compile/check/run tasks.
- `scml.diagnostics.enabled`: enable language-service diagnostics.
- `scml.diagnostics.compileOnSave`: run compiler diagnostics when saved `.scml` files are clean.

## Suggested pure-SCML FFI workflow

1. Select the modern SCML CLI with `SCML: Select CLI Executable`.
2. Create a `.scmlh` wrapper around FFI boilerplate (`ffi.add_search_path`, `ffi.load`, `ffi.declare`).
3. Use snippets such as `ffiload`, `ffideclare`, and `scffiheader` to scaffold wrappers.
4. Keep application files focused on SCML logic and call wrapper macros instead of raw FFI calls.
