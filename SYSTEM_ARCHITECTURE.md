# OpenSCM / SCML – Arquitectura seria del sistema

Este documento define una arquitectura de trabajo para evolucionar SCML de un prototipo mínimo hacia un sistema completo de compilación, runtime y tooling.

## 1) Capas del sistema

1. **Frontend**
   - `lexer/`: tokenización.
   - `parser/`: AST y reglas sintácticas.
   - `compiler/`: lowering de AST a bytecode/opcodes.

2. **Core Runtime / VM**
   - `vm/`: ciclo de ejecución, stack, memoria y control de flujo.
   - `opcode/`: registro y despacho de opcodes.

3. **Std + ISO profile**
   - `stscm/std.scmlh`: macros de alto nivel (flujo, RTTI, clases, colecciones).
   - `scmlspec/fixtures/scml_iso_advanced_preprocessed.scmlh`: perfil ISO y capacidades declarativas.

4. **Tooling y DX**
   - `tools/scml_doctor.sh`: diagnóstico reproducible del entorno.
   - `tests/smoke_suite.sh`: smoke test de compilación de ejemplos críticos.
   - `vscode-scml-extension/`: soporte IDE.

## 2) Principios de arquitectura

- **Compatibilidad incremental:** cambios del std/ISO deben ser compatibles con VM existente cuando sea posible.
- **Contratos explícitos:** cada macro o capa expone su contrato (inputs/outputs/errores).
- **Tooling primero:** toda feature mayor debe venir con prueba y comando reproducible.
- **Separación de responsabilidades:** no mezclar parsing, semántica y ejecución en una sola capa.

## 3) Pipeline recomendado

1. `make bin/scml`
2. `tests/smoke_suite.sh`
3. `tools/scml_doctor.sh`

Esto garantiza compilador funcional, ejemplos clave compilables y diagnóstico del sistema.

## 4) Roadmap técnico (serio)

- **Fase A: Frontend fuerte**
  - AST tipado para clases/RTTI/flow avanzado.
  - Mejor manejo de errores con spans y contexto.

- **Fase B: IR intermedio**
  - Introducir IR para optimizaciones (const-folding, dead-branch pruning, peephole).

- **Fase C: Runtime robusto**
  - Canales de error estructurados.
  - Instrumentación (trace, perf counters, debug hooks).

- **Fase D: Ecosistema**
  - Package manager SCMR sólido.
  - LSP completo + formatter + linter.

## 5) Criterios de calidad

Una feature no se considera “lista” sin:
- Pruebas smoke o unitarias.
- Documentación mínima de arquitectura/uso.
- Integración por `make`/scripts estándar.
