# SCML STD modular congelada

Este documento congela la STD global actual y define el sistema oficial de módulos de SCML. A partir de esta estructura, la biblioteca estándar ya no crece mediante funciones globales sueltas: toda función nueva debe tener dominio, módulo, propósito único y revisión explícita.

## Estado de la STD

- **STD global congelada:** `stscm/std.scmlh` conserva los macros históricos para compatibilidad, pero no acepta funciones nuevas.
- **SCML_* obsoleto:** los helpers `SCML_*` viven en `stscm/compat/legacy_scml_prefix.scmlh`, están deprecados y solo existen para migración.
- **Módulos oficiales:** los equivalentes modernos viven en `stscm/modules/*.scmlh` con nombres `lower_snake_case` sin prefijo global `SCML_`.
- **Compatibilidad no predeterminada:** si un proyecto nuevo necesita compatibilidad antigua, debe documentar por qué importa `std.scmlh` o `compat/legacy_scml_prefix.scmlh`.

## Capas oficiales

| Capa | Ruta | Responsabilidad | Regla |
| --- | --- | --- | --- |
| `core` | `opcode/`, `vm/`, macros primitivos en `stscm/std.scmlh` | núcleo mínimo estable: opcodes, control de flujo, memoria básica, tipos mínimos | no depende de librerías ni backends externos |
| `runtime` | `runtime/`, `stscm/modules/vm.scmlh`, `stscm/modules/concurrency.scmlh` | módulos host-backed y estado de ejecución | expone capacidades del host sin mezclar VM y STD |
| `libraries` | `stscm/modules/*.scmlh` | IO, colecciones, tipos, rangos, datos, filesystem | cada archivo es un módulo de dominio único |
| `external` | `ffi/`, `stscm/modules/ffi.scmlh`, integraciones GTA | FFI e integraciones externas | nunca forma parte del núcleo mínimo |
| `compat` | `stscm/compat/legacy_scml_prefix.scmlh` | puentes antiguos y deprecados | congelado; no define el estándar moderno |

## Núcleo mínimo estable del lenguaje

El núcleo estable es pequeño y se limita a:

1. labels, opcodes numéricos y saltos;
2. `SET`/`LOAD`/`PUSH_*`;
3. aritmética básica `ADD`/`SUB`/`MUL`/`DIV`;
4. comparaciones y salto condicional `IF_*`;
5. llamadas `CALL`/`RETURN` y fin de hilo;
6. memoria heap/array básica;
7. eventos mínimos `BIND_EVENT`/`TRIGGER_EVENT`;
8. registro de módulos nativos por VM/runtime.

Todo lo demás es librería o runtime, no core.

## Clasificación de funciones existentes por dominio

| Dominio | Funciones/macros representativas | Módulo moderno |
| --- | --- | --- |
| Core VM/control | `NOP`, `HALT`, `SET`, `LOAD`, `CALL`, `RETURN`, `JUMP`, `WAIT`, `IF_EQ`, `IF_NE`, `IF_GT`, `IF_LT` | core congelado en `stscm/std.scmlh` |
| Core math/string | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `STRCAT`, `TO_INT`, `TO_FLOAT`, `STRLEN`, `SUBSTR` | core/libraries según uso |
| Memoria/arrays | `HEAP_ALLOC`, `HEAP_FREE`, `HEAP_READ`, `HEAP_WRITE`, `ARRAY_NEW`, `ARRAY_GET`, `ARRAY_SET`, `ARRAY_LEN` | core memory |
| IO/console | `PRINT`, `LOG`, `SCML_COUT`, `SCML_CIN`, `CONSOLE_*` | `stscm/modules/io.scmlh` |
| Colecciones | `VECTOR_*`, `MAP_*`, `SCML_VECTOR_*`, `SCML_MAP_*`, `SCML_LIST_*` | `stscm/modules/collections.scmlh` |
| Tipos/RTTI | `TYPE_*`, `IS_TYPE`, `RTTI_*`, `OPTIONAL_*`, `SCML_OPTIONAL_*`, `SCML_EXPECTED_*`, `SCML_ANY_*` | `stscm/modules/types.scmlh` |
| Clases/objetos | `CLASS_*`, `OBJECT_NEW`, `FIELD_SET`, `FIELD_GET`, `METHOD_CALL*` | library/object model; no nuevas APIs sin módulo dedicado |
| Filesystem | `FILE_READ`, `FILE_WRITE`, `SCML_FILESYS_*`, `APP_READ_TEXT`, `APP_WRITE_TEXT` | `stscm/modules/fs.scmlh` |
| Concurrencia | `ASYNC_*`, `THREAD_*`, `SCML_THREAD_*`, `SCML_CORO_*` | `stscm/modules/concurrency.scmlh` |
| Rangos/algoritmos | `ITER_*`, `ALG_*`, `SCML_RANGE_*`, `SCML_RANGES_*` | `stscm/modules/ranges.scmlh` |
| Datos/env/json | `SCML_DATA_*`, `SCML_JSON_GET`, `SCML_ENV_*` | `stscm/modules/data.scmlh` |
| FFI/external | `SCML_FFI_*` | `stscm/modules/ffi.scmlh` |
| VM/metadatos | `SCML_VM_TRACE`, `SCML_JIT_HINT`, `SCML_MODULE_EXPORT` | `stscm/modules/vm.scmlh` y `stscm/modules/meta.scmlh` |

## Equivalentes modernos para funciones obsoletas

| Obsoleto | Moderno | Módulo |
| --- | --- | --- |
| `SCML_COUT` | `cout` | `io.scmlh` |
| `SCML_CIN` | `cin` | `io.scmlh` |
| `SCML_VECTOR_NEW` | `vector_create` | `collections.scmlh` |
| `SCML_VECTOR_PUSH` | `vector_append` | `collections.scmlh` |
| `SCML_VECTOR_GET` | `vector_at` | `collections.scmlh` |
| `SCML_VECTOR_SIZE` | `vector_count` | `collections.scmlh` |
| `SCML_LIST_NEW` / `SCML_LIST_PUSH` | `list_create` / `list_append` | `collections.scmlh` |
| `SCML_MAP_NEW` / `SCML_MAP_PUT` | `map_create` / `map_put_i32` | `collections.scmlh` |
| `SCML_OPTIONAL_*` | `optional_*` | `types.scmlh` |
| `SCML_EXPECTED_*` | `expected_*` | `types.scmlh` |
| `SCML_VARIANT_SET` | `variant_set` | `types.scmlh` |
| `SCML_ANY_SET` / `SCML_ANY_VALUE` | `any_set` / `any_value` | `types.scmlh` |
| `SCML_FILESYS_*` | `path_set`, `file_exists` | `fs.scmlh` |
| `SCML_THREAD_*` / `SCML_CORO_*` | `thread_*`, `coro_*` | `concurrency.scmlh` |
| `SCML_RANGE_*` / `SCML_RANGES_*` | `range_*` | `ranges.scmlh` |
| `SCML_DATA_*`, `SCML_JSON_GET`, `SCML_ENV_*` | `data_*`, `json_get_value`, `env_*` | `data.scmlh` |
| `SCML_FFI_*` | `ffi_*` | `ffi.scmlh` |
| `SCML_VM_TRACE`, `SCML_JIT_HINT` | `vm_trace_set`, `jit_hint` | `vm.scmlh` |
| `SCML_MODULE_EXPORT` | `module_export` | `meta.scmlh` |

## Reglas estrictas de naming

1. Funciones nuevas: `lower_snake_case`.
2. Constantes: `UPPER_SNAKE_CASE`, solo si pertenecen a core o al módulo que las define.
3. Prohibido crear nuevas funciones `SCML_*` como estándar.
4. Prohibido mezclar camelCase y snake_case para la misma operación; se conserva solo como compatibilidad deprecada.
5. El nombre debe incluir el sustantivo del dominio cuando sea necesario para evitar colisiones (`range_size`, `file_exists`, `ffi_memmove`).
6. Cada función debe tener un propósito único; si combina efectos, debe dividirse antes de entrar a STD.
7. Una función nueva debe tener ejemplo oficial y entrada en este documento.

## Duplicados funcionales detectados

- `SCML_VECTOR_*`, `VECTOR_*` y `vector_*` apuntaban al mismo modelo de vector. El estándar moderno usa `vector_create`, `vector_append`, `vector_at`, `vector_count`.
- `SCML_COUT` duplicaba `PRINT`/`CONSOLE_WRITE`. El estándar moderno separa `cout` para impresión VM y `console_write_text` para runtime host.
- `SCML_FILESYS_EXISTS` duplicaba wrappers runtime de archivos. El estándar moderno usa `file_exists`.
- `SCML_RANGE_I32` y `SCML_RANGES_IOTA` duplicaban construcción de rangos. El estándar moderno conserva `range_i32` y alias explícito `range_iota`.
- `SCML_FFI_*` era un prefijo global único para FFI. El estándar moderno usa `ffi_*` dentro del módulo FFI.

Los duplicados históricos no se eliminan físicamente si rompen compatibilidad; se mueven a `compat` y quedan deprecados.

## Proceso de revisión antes de añadir funciones

Antes de aceptar una función nueva en STD:

1. asignar capa (`core`, `runtime`, `libraries`, `external`, `compat`);
2. asignar módulo concreto bajo `stscm/modules/`;
3. comprobar que no duplica una función existente;
4. justificar propósito único y firma;
5. verificar nombre `lower_snake_case`;
6. añadir ejemplo en `examples/std_modules/`;
7. actualizar este documento;
8. ejecutar `tools/scml_migration_audit.sh` y `tests/smoke_suite.sh`.

## Regla de dependencias cruzadas

Un módulo puede usar core. Un módulo de librería no debe importar otro módulo completo salvo necesidad documentada; si necesita una primitiva, debe depender de la macro core subyacente. `stscm/modules/std_modules.scmlh` existe solo como importador de conveniencia.
