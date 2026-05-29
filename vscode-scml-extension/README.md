# SCML Advanced Tools (VS Code)

A mature IDE layer for SCML/SCMLH projects. The extension provides LSP-style language intelligence without requiring a separate server process.

## Language intelligence

- Syntax highlighting for labels, directives, strings, numbers, keywords, and numeric opcodes.
- IntelliSense completions for numeric opcodes, opcode names, standard macros, labels, directives, and type names.
- Hover documentation for opcodes, macros, labels, and SCML static type names.
- Go to Definition for labels, macros, defines, and variables.
- Find References and Rename Symbol for label-like SCML symbols.
- Document symbols and folding ranges for labels and macro blocks.
- Document formatting for SCML-style indentation and argument spacing.
- Quick Fix to create missing labels from unresolved `@LABEL` diagnostics.
- Problems diagnostics for duplicate labels, unresolved labels, missing includes, unknown opcodes/macros, and compiler errors on save.

## Build/run workflow

- `SCML: Compile Current File`
- `SCML: Run Current Binary`
- `SCML: Compile and Run Current File`
- `SCML: Restart Language Services`

The extension resolves the SCML executable from `scml.executablePath`, workspace `bin/scml`, workspace `scml`, or `PATH`.

## Settings

- `scml.executablePath`: absolute path to the SCML CLI.
- `scml.defaultBinOutputDir`: output directory for `.scmlbin` files. Supports `${workspaceFolder}`.
- `scml.runWithTrace`: include `--trace` when running binaries.
- `scml.diagnostics.enabled`: enable language-service diagnostics.
- `scml.diagnostics.compileOnSave`: run compiler diagnostics when saved `.scml` files are clean.
