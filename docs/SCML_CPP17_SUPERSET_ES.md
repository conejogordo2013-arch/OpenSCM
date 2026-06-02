# SCML C++17 Superset Pack

Este paquete sube la superficie moderna de SCML hacia un perfil práctico que compite con C++17 en los puntos que más importan para scripting embebido: sintaxis moderna, std helpers, runtime hostless y tooling reproducible.

## Capacidades añadidas

- **Atributos C++17-style**: el parser acepta y elimina atributos `[[...]]` antes de bajar la línea a bytecode.
- **Inline/constexpr/static declarations**: declaraciones como `inline constexpr INT value = 17;` se aceptan como contratos de fuente y se bajan al sistema `TYPE_DECL` + `SET` existente.
- **Structured bindings reales para agregados SCML**: `AUTO [a, b, c] = ref;` ahora lee `ref[0]`, `ref[1]`, `ref[2]` mediante `0B12` en vez de inicializar los nombres a cero.
- **If initializer estilo C++17**: `if (let $x: i32 = 17; $x == expected) { ... }` emite primero la declaración y después la condición.
- **`if constexpr` práctico**: condiciones literales o comparaciones enteras constantes seleccionan el bloque en preprocesado; una rama falsa se elimina con `MODERN_BLOCK_SKIP`.
- **Fold helpers**: `fold_add(...)`, `fold_mul(...)`, `fold_any(...)` y `fold_all(...)` se reducen a opcodes aritméticos/bitwise.
- **Std C++17 refinada**: helpers para `optional` (`SCML_OPTIONAL_HAS_VALUE`, `SCML_OPTIONAL_VALUE`), `variant` (`SCML_VARIANT_TAG`, `SCML_VARIANT_VALUE`), `any` y `filesystem exists` host-backed.

## Perfil de victoria práctica contra C++17

SCML sigue sin pretender ser un clon ISO de C++: su ventaja es integrar VM, bytecode, eventos, hot reload, módulos nativos, async cooperativo, scripts hostless y tooling en un único stack. Con este pack, la superficie que antes era principalmente declarativa para varias features C++17 ahora tiene lowering ejecutable y smoke test reproducible.

Ejemplo canónico:

```sh
bin/scml compile examples/cpp17_superiority.scml .scml/cpp17_superiority.scmlbin
bin/scml run .scml/cpp17_superiority.scmlbin
```
