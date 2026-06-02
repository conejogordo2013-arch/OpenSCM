# SCML C++20 Domination Pack

Este pack continúa el salto iniciado por C++17 y añade una capa práctica estilo C++20 enfocada a lo que hace fuerte a SCML: bajar sintaxis moderna a bytecode, mantener scripts embebidos simples y sumar runtime/tooling integrado.

## Capacidades nuevas

- **Range-for ejecutable**: `for (AUTO item : range)` lee rangos SCML `[begin, end)` y genera el loop con labels, condición, variable de iteración y post-incremento.
- **Ranges/views de std**: `SCML_RANGES_IOTA`, `SCML_RANGES_SIZE`, `SCML_RANGES_TRANSFORM_ADD`, `SCML_RANGES_TAKE` y `SCML_RANGES_DROP` construyen pipelines simples sobre rangos de enteros.
- **Concepts/requires ejecutable**: `requires(value, "i32") -> $ok` baja a `TYPE_ASSERT`, y `SCML_CONCEPT_SATISFIES`/`SCML_REQUIRES` dan una superficie de std equivalente.
- **Coroutines prácticas**: `co_await task -> done` baja a `ASYNC_DONE`; `co_return value` escribe `$RETVAL` y termina el task/thread con `END_THREAD`.
- **Consteval helpers**: `consteval_add/sub/mul/div` bajan directamente a opcodes aritméticos deterministas, útiles para expresar intención C++20 sin abandonar el bytecode actual.
- **Modules/import metadata**: `import module.name;` se acepta como metadata moderna para proyectos grandes, manteniendo `use` para imports reales de headers.

## Ejemplo canónico

```sh
bin/scml compile examples/cpp20_domination.scml .scml/cpp20_domination.scmlbin
bin/scml run .scml/cpp20_domination.scmlbin
```

Salida esperada:

```text
5
10
33
45
1
42
```

## Qué significa “dominar” aquí

No intenta copiar cada regla ISO de C++20. La estrategia es ganar en el terreno de SCML: scripts con sintaxis moderna, lowering reproducible a VM, eventos/async integrados, std por macros, runtime embebido y smoke tests que prueban que las features realmente corren.
