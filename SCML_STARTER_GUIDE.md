# Guía de arranque SCML (OpenSCM)

Este repositorio ya trae casi todo lo necesario para empezar un proyecto real en SCML.

## 1) Flujo base del lenguaje

1. Escribes `.scml` y opcionalmente `.scmlh`.
2. `lexer/` tokeniza.
3. `parser/` resuelve includes/macros/labels.
4. `compiler/` convierte a `.scmlbin`.
5. `vm/` ejecuta bytecode.

## 2) Archivos clave para estudiar

- `README.md`: visión global de sintaxis, opcodes y runtime.
- `std.scmlh`: macros estándar.
- `examples/`: biblioteca práctica del lenguaje.
- `examples/rtti_pointers_complex.scml` + `.scmlh`: RTTI + punteros + control de flujo complejo.
- `examples/really_complex.scml`: flujo, eventos, heap y funciones.

## 3) Primera tarea sugerida (tu objetivo)

Para empezar tu proyecto con una base fuerte:

- Header complejo: `examples/scml_project_header_complex.scmlh`
- Programa complejo: `examples/scml_project_program_complex.scml`

Estos dos archivos sirven como plantilla inicial basada en el ejemplo más completo de RTTI/punteros del repo.

## 4) Comandos mínimos de trabajo

```bash
make
bin/scml compile examples/scml_project_program_complex.scml examples/scml_project_program_complex.scmlbin
bin/scml run examples/scml_project_program_complex.scmlbin --trace
```

## 5) Qué dominar para programar “en serio”

- **Control de flujo**: `00D6`, `00D8`, `00D9`, saltos a labels.
- **Memoria/“punteros” seguros**: `0B14` (alloc array), `0B12` (read), `0B13` (write), `0B11` (free).
- **RTTI práctico**: usar arrays paralelos (`TYPE`, `STATE`, `PTR`) por entidad.
- **Funciones**: `CALL @FUNC` + `0D01:` return.
- **Eventos**: `EVENT_BIND` y `TRIGGER_EVENT` para arquitectura modular.

## 6) Recomendación inmediata

Comienza copiando el patrón de `scml_project_program_complex.scml`, luego:

1. Renombra tipos/estados a tu dominio (NPC, ítems, misiones, etc.).
2. Añade un loop principal con `ON_TICK` o equivalente.
3. Encapsula la lógica en funciones por subsistema.
4. Usa `.scmlh` para macros/utilidades compartidas.
