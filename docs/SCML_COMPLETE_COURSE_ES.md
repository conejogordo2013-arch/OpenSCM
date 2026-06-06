# Curso completo de SCML desde cero

Este curso está pensado para aprender SCML de forma práctica usando OpenSCM: el compilador, la VM, la librería estándar, los ejemplos y las pruebas del repositorio. Empieza desde cero, pero avanza hasta memoria, eventos, tipos, módulos, integración con C/C++ y terminal rendering.

## 0. Qué es SCML y qué problema resuelve

SCML significa **Scripting Control Markup Language**. En OpenSCM es un lenguaje compacto con estética de scripts SCM/GTA San Andreas, pero ejecutado sobre una VM moderna escrita en C.

La idea principal es escribir programas como una secuencia de **labels** y **opcodes**:

```scml
:MAIN
0004: $COUNTER 0
:LOOP
03E5: $COUNTER
0006: $COUNTER $COUNTER 1
00D9: $COUNTER 5 @LOOP
0001:
```

Conceptos clave:

- `:MAIN` y `:LOOP` son labels.
- `0004:` guarda un valor en una variable.
- `03E5:` imprime.
- `0006:` suma.
- `00D9:` salta si el primer valor es menor que el segundo.
- `0001:` termina el script.

SCML no intenta parecer Python, JavaScript o Lua. Su personalidad está en el flujo explícito tipo opcode, ideal para scripting embebido, lógica de juego, prototipos de VM, herramientas y automatización controlada.

## 1. Instalación y primer programa

### 1.1 Compilar la herramienta

Desde la raíz del repositorio:

```bash
make
```

Esto construye `bin/scml`, la CLI que compila y ejecuta scripts.

### 1.2 Hello World mínimo

Crea `hello.scml`:

```scml
:MAIN
03E5: "Hola SCML"
0001:
```

Compila y ejecuta:

```bash
bin/scml compile hello.scml hello.scmlbin
bin/scml run hello.scmlbin
```

Salida esperada:

```text
Hola SCML
```

## 2. Modelo mental: pipeline completo

SCML pasa por estas capas:

1. **Lexer**: convierte líneas en tokens.
2. **Parser/preprocesador**: procesa includes, defines, macros y labels.
3. **Compiler**: resuelve labels y emite bytecode `.scmlbin`.
4. **Opcode registry**: traduce códigos SCM y nombres a opcodes internos.
5. **VM**: ejecuta el bytecode, variables, stack, eventos, heap y llamadas nativas.

Este flujo importa porque los errores pueden aparecer en distintas fases:

- Error de sintaxis/token: lexer/parser.
- Label inexistente u operandos inválidos: compilador.
- División por cero, acceso fuera de rango o shift inválido: runtime/VM.
- Función nativa inexistente: integración host/runtime.

## 3. Sintaxis base

### 3.1 Labels

Un label empieza con `:`:

```scml
:MAIN
:UPDATE
:FUNC_CALCULATE_DAMAGE
```

Para saltar a un label se usa `@LABEL`:

```scml
000A: @UPDATE
```

### 3.2 Variables

Hay dos estilos frecuentes:

```scml
0004: $GLOBAL_SCORE 10
0004: 0@ 25
```

- `$NOMBRE` se usa como variable global o compartida.
- `0@`, `1@`, etc. se parecen a slots locales/temporales de estilo SCM.
- Dentro de llamadas, las variables no `$` pueden ser locales al frame activo.

### 3.3 Literales

SCML soporta enteros, floats y strings:

```scml
0004: $I 42
0004: $F 3.14
0004: $S "texto"
0004: $NEG -1.5
```

### 3.4 Comentarios

Usa `;` para comentarios:

```scml
; Esto es un comentario
03E5: "visible"
```

## 4. Opcodes imprescindibles

| Código/nombre | Uso | Ejemplo |
|---|---|---|
| `0000:` / `NOP` | no hace nada | `0000:` |
| `0001:` / `HALT` | termina | `0001:` |
| `0004:` / `SET` | asigna | `0004: $X 10` |
| `0006:` / `ADD` | suma | `0006: $Z $X 1` |
| `0007:` / `SUB` | resta | `0007: $Z $X 1` |
| `0008:` / `MUL` | multiplica | `0008: $Z $X 2` |
| `0009:` / `DIV` | divide | `0009: $Z $X 2` |
| `000A:` / `JUMP` | salto | `000A: @LOOP` |
| `000B:` / `WAIT` | espera/yield | `000B: 16` |
| `03E5:` / `PRINT` | imprime con salto de línea | `03E5: "ok"` |
| `0D00:` / `CALL` | llama label | `0D00: @FUNC` |
| `0D01:` / `RETURN` | vuelve | `0D01:` |
| `0D02:` / `END_THREAD` | termina thread/event handler | `0D02:` |

## 5. Control de flujo

### 5.1 Saltos incondicionales

```scml
:MAIN
03E5: "inicio"
000A: @FIN
03E5: "esto no se imprime"
:FIN
03E5: "fin"
0001:
```

### 5.2 Condicionales

SCML usa saltos condicionales:

```scml
:MAIN
0004: $HP 50
00D8: $HP 25 @VIVO
03E5: "derrotado"
0001:
:VIVO
03E5: "sigue vivo"
0001:
```

Condiciones principales:

- `00D6:` / `IF_EQ`: igual.
- `00D7:` / `IF_NE`: distinto.
- `00D8:` / `IF_GT`: mayor.
- `00D9:` / `IF_LT`: menor.
- `0B25:` / `IF_GE`: mayor o igual.
- `0B26:` / `IF_LE`: menor o igual.

### 5.3 Bucles

```scml
:MAIN
0004: $I 0
:LOOP
03E5: $I
0006: $I $I 1
00D9: $I 5 @LOOP
0001:
```

Patrón mental:

1. Inicializa contador.
2. Ejecuta cuerpo.
3. Actualiza contador.
4. Condición que vuelve al inicio.

## 6. Macros y librería estándar

SCML puede usar `.scmlh` como headers con macros. La librería estándar raíz está en `std.scmlh` e incluye `stscm/std.scmlh`.

Ejemplo con macros:

```scml
#include "../std.scmlh"

:MAIN
SET($A, 10)
ADD($B, $A, 5)
PRINT($B)
HALT()
```

Las macros no son magia de runtime: normalmente expanden a opcodes existentes. Por eso ayudan a escribir código más legible sin abandonar el estilo SCML.

### 6.1 Macros base útiles

- `SET(var, value)`
- `ADD(out, a, b)`
- `SUB(out, a, b)`
- `MUL(out, a, b)`
- `DIV(out, a, b)`
- `PRINT(value)`
- `JUMP(label)`
- `IF_EQ(a, b, label)`
- `CALL(label)`
- `RETURN()`
- `END_THREAD()`

### 6.2 Includes

```scml
#include "../std.scmlh"
#include "mi_header.scmlh"
```

Recomendación: usa `.scmlh` para constantes, macros y APIs compartidas del proyecto.

## 7. Funciones y llamadas

SCML usa labels como funciones:

```scml
#include "../std.scmlh"

:MAIN
SET($A, 7)
SET($B, 5)
CALL(@SUMAR)
PRINT($RET)
HALT()

:SUMAR
ADD($RET, $A, $B)
RETURN()
```

Buenas prácticas:

- Define claramente qué variables son entrada y salida.
- Usa `$RET`, `$RETVAL` o una variable de salida documentada.
- Termina funciones con `RETURN()`.
- Termina handlers/eventos con `END_THREAD()`.

## 8. Memoria, arrays y referencias seguras

OpenSCM evita exponer punteros crudos. Usa handles enteros a objetos del heap.

Opcodes y macros importantes:

- `0B10` / `HEAP_ALLOC(size, outvar)`
- `0B11` / `HEAP_FREE(ref)`
- `0B12` / `HEAP_READ(out, ref, index)`
- `0B13` / `HEAP_WRITE(ref, index, value)`
- `0B14` / `ARRAY_NEW(size, outvar)`
- `ARRAY_LEN(out, ref)`

Ejemplo:

```scml
#include "../std.scmlh"

:MAIN
ARRAY_NEW(3, $ARR)
ARRAY_SET($ARR, 0, 10)
ARRAY_SET($ARR, 1, 20)
ARRAY_SET($ARR, 2, 30)
ARRAY_GET($VALUE, $ARR, 1)
PRINT($VALUE)
HEAP_FREE($ARR)
HALT()
```

Salida:

```text
20
```

Reglas prácticas:

- Reserva antes de escribir.
- No escribas fuera de rango.
- Libera cuando el objeto ya no se use.
- Trata los handles como referencias opacas, no como direcciones reales.

## 9. Strings, conversión y matemáticas

SCML incluye utilidades para strings y números:

```scml
#include "../std.scmlh"

:MAIN
STRCAT($MSG, "Hola ", "SCML")
PRINT($MSG)
STRLEN($LEN, $MSG)
PRINT($LEN)
TO_INT($I, "123")
ADD($NEXT, $I, 1)
PRINT($NEXT)
HALT()
```

Más herramientas:

- `MOD(out, a, b)`
- `TO_FLOAT(out, value)`
- `POW(out, a, b)`
- `SIN`, `COS`, `TAN`, `SQRT`, `ATAN2`
- `FLOOR`, `CEIL`, `ROUND`, `ABS`
- `SUBSTR(out, s, idx, len)`
- `STR_REPEAT(out, text, count)`

## 10. Tipado estático/contratos de tipo

El nivel moderno de SCML incluye declaraciones con comprobación del compilador:

```scml
#include "../std.scmlh"

:MAIN
LET_I32($COUNT, 1)
LET_F32($SPEED, 3.5)
LET_STR($NAME, "SCML")
TYPE_ASSERT($COUNT, "i32", $OK)
PRINT($OK)
HALT()
```

Si declaras `$COUNT` como `i32` y luego intentas guardar un string, el compilador debe rechazarlo. Esto permite escribir scripts más seguros sin perder el control explícito.

## 11. Eventos y arquitectura modular

SCML tiene registro de eventos con varios handlers:

```scml
#include "../std.scmlh"

:MAIN
BIND_EVENT("ON_START", @ON_START)
TRIGGER_EVENT("ON_START")
HALT()

:ON_START
PRINT("evento recibido")
END_THREAD()
```

Úsalo para:

- `ON_START`: inicialización.
- `ON_TICK`: actualización periódica.
- `ON_DAMAGE`, `ON_PICKUP`, `ON_MISSION_COMPLETE`: lógica de juego.
- Eventos internos de sistemas: inventario, UI, audio, red.

Patrón recomendado:

```text
MAIN
  inicializa estado
  registra eventos
  dispara evento inicial

handlers
  hacen una tarea pequeña
  terminan con END_THREAD

funciones
  encapsulan cálculos reutilizables
```

## 12. Async cooperativo

SCML soporta tareas cooperativas sobre la cola de eventos de la VM:

```scml
#include "../std.scmlh"

:MAIN
ASYNC_SPAWN(@TASK_WORK, $TASK)
:WAIT_TASK
ASYNC_AWAIT_POLL($TASK, @DONE, @PENDING)
:PENDING
ASYNC_YIELD_MS(1)
JUMP(@WAIT_TASK)
:DONE
PRINT("task terminada")
HALT()

:TASK_WORK
PRINT("trabajando")
END_THREAD()
```

Esto no convierte SCML en un sistema multithread real, pero sí permite estructurar esperas y trabajo diferido sin bloquear la lógica principal.

## 13. Terminal rendering y salida avanzada

SCML tiene opcodes para terminal:

- `PRINT_RAW(v)`
- `CONSOLE_CLEAR()`
- `CONSOLE_COLOR(fg)`
- `CONSOLE_COLOR_BG(fg, bg)`
- `CONSOLE_RESET()`
- `CONSOLE_MOVE(row, col)`
- `CONSOLE_ERASE_LINE()`
- `CONSOLE_STYLE(style)`
- `CONSOLE_RENDER_SPAN(buffer, width, height)`

Ejemplo simple:

```scml
#include "../std.scmlh"

:MAIN
CONSOLE_CLEAR()
CONSOLE_COLOR(46)
PRINT_RAW("SCML verde")
CONSOLE_RESET()
HALT()
```

Para animación, estudia los ejemplos de cubos ASCII y rendering por spans. La idea es evitar imprimir carácter por carácter cuando se puede preparar un frame o una línea completa.

## 14. Spans/framebuffer ligero

Los spans son buffers byte-oriented útiles para renderizar texto o grids:

```scml
:MAIN
0B43: 4 $BUF      ; SPAN_CREATE size=4
0B44: $BUF        ; SPAN_PIN
0B45: $BUF 0 4 46 ; fill con '.'
0B46: $BUF 1 64   ; escribe '@'
0B48: $BUF 2 2    ; render 2x2
0001:
```

Salida esperada:

```text
.@
..
```

La VM valida rangos y exige pinning para renderizar, lo que mejora la seguridad.

## 15. Integración con host y funciones nativas

`CALL_NATIVE` permite llamar APIs registradas por el runtime o por un programa embebedor:

```scml
#include "../std.scmlh"

:MAIN
CALL_NATIVE("console.write", "hola desde native")
CALL_NATIVE("console.flush", 0)
HALT()
```

La librería estándar trae wrappers para módulos como:

- `runtime.wait`
- `runtime.get_time_ms`
- `console.write`
- `console.flush`
- `file.read_txt`
- `file.write_txt`
- `gpu.*` cuando hay backend disponible
- `audio.*`
- `net.*`

En C/C++, el host puede cargar bytecode, registrar funciones nativas, disparar eventos y ejecutar/actualizar la VM.

## 16. Estructuras de datos de alto nivel

Aunque SCML es de bajo nivel, la std construye abstracciones con heap/macros:

- `STRUCT_NEW`, `FIELD_SET`, `FIELD_GET`
- `CLASS_NEW`, `OBJ_KIND`, `OBJ_FIELD_COUNT`
- `VECTOR_NEW`, `VECTOR_PUSH`, `VECTOR_GET`, `VECTOR_SIZE`
- `MAP_NEW`, `MAP_PUT`, `MAP_GET_OR`
- `OPTIONAL_T_*`
- `VECTOR_T_*` con tags de tipo
- `RTTI_TAG_SET`, `RTTI_ASSERT_KIND`

Ejemplo vector:

```scml
#include "../std.scmlh"

:MAIN
VECTOR_NEW(4, $V)
VECTOR_PUSH($V, 10)
VECTOR_PUSH($V, 20)
VECTOR_GET($X, $V, 1)
PRINT($X)
HEAP_FREE($V)
HALT()
```

## 17. Proyecto recomendado desde cero

Estructura sugerida:

```text
mi_proyecto/
  main.scml
  game.scmlh
  inventory.scmlh
  missions.scmlh
  build.sh
```

`game.scmlh`:

```scml
#include "../std.scmlh"

#define STATE_BOOT 0
#define STATE_RUNNING 1
#define STATE_DONE 2

macro PRINT_SECTION(name):
    PRINT("===")
    PRINT(name)
    PRINT("===")
endmacro
```

`main.scml`:

```scml
#include "game.scmlh"

:MAIN
PRINT_SECTION("BOOT")
LET_I32($STATE, STATE_BOOT)
ARRAY_NEW(8, $INVENTORY)
BIND_EVENT("ON_TICK", @ON_TICK)
TRIGGER_EVENT("ON_TICK")
HEAP_FREE($INVENTORY)
HALT()

:ON_TICK
PRINT("tick")
SET($STATE, STATE_RUNNING)
END_THREAD()
```

Compilación:

```bash
bin/scml compile mi_proyecto/main.scml mi_proyecto/main.scmlbin
bin/scml run mi_proyecto/main.scmlbin --trace
```

## 18. Depuración y diagnóstico

Herramientas útiles:

```bash
make test
make doctor
bin/scml run script.scmlbin --trace
bin/scml run script.scmlbin --dump-memory
```

Consejos:

- Si algo salta al label equivocado, ejecuta con `--trace`.
- Si falla memoria, revisa índices y tamaños de `ARRAY_NEW`/`HEAP_ALLOC`.
- Si un macro genera código raro, reduce el caso a opcodes puros.
- Si una función no vuelve, busca `RETURN()` ausente.
- Si un handler se comporta raro, revisa que termine con `END_THREAD()`.

## 19. Prácticas por niveles

### Nivel 1: base

1. Imprime tres mensajes.
2. Crea variables `$A`, `$B`, `$SUM`.
3. Suma y muestra resultado.
4. Haz un loop de 0 a 9.

### Nivel 2: control

1. Crea un sistema de vida `HP`.
2. Si `HP > 0`, imprime vivo.
3. Si `HP <= 0`, imprime derrotado.
4. Añade una función `APPLY_DAMAGE`.

### Nivel 3: memoria

1. Crea un array de inventario de 5 slots.
2. Guarda IDs de objetos.
3. Lee el slot 2.
4. Libera el array.

### Nivel 4: eventos

1. Registra `ON_START` y `ON_TICK`.
2. En `ON_START`, inicializa estado.
3. En `ON_TICK`, actualiza contador.
4. Divide la lógica en funciones.

### Nivel 5: tipos y std

1. Usa `LET_I32`, `LET_STR`, `TYPE_ASSERT`.
2. Crea un vector con `VECTOR_NEW`.
3. Implementa búsqueda con `ALG_FIND_EQ` o un loop propio.
4. Añade errores explícitos con `TRY_BEGIN`, `THROW`, `CATCH_IF`.

### Nivel 6: aplicación personal completa

Construye una mini app de consola:

- Menú textual.
- Estado guardado en arrays/mapas.
- Comandos del usuario con `INPUT`.
- Render con `PRINT_RAW`/colores.
- Persistencia con wrappers de archivo si el runtime lo permite.

## 20. ¿Puede SCML hacer cosas realmente complejas?

Sí, **puede hacer cosas realmente complejas**, especialmente si hablamos de lógica embebida, simulaciones, prototipos de juegos, sistemas de eventos, automatización controlada, terminal rendering, estructuras de datos sobre heap, integración con C/C++ y scripting de runtime.

Razones:

- Tiene compilador y VM propios.
- Tiene labels, saltos, funciones, stack de llamadas y eventos.
- Tiene heap seguro por handles, arrays, lectura/escritura y validaciones.
- Tiene una std amplia construida con macros.
- Tiene llamadas nativas para extender capacidades fuera de la VM.
- Tiene pruebas smoke que compilan ejemplos y validan regresiones reales.
- Tiene tooling de diagnóstico, tracing, debugger/hot reload y soporte editor en desarrollo.

Pero hay que decirlo con honestidad: SCML no busca competir como lenguaje generalista cómodo contra Python, TypeScript, Rust o C++. Su fuerza está en ser un lenguaje explícito, embebible y controlable. Para proyectos enormes con mucha ergonomía, paquetes, concurrencia real, tooling maduro y tipos ricos, todavía necesitaría crecer bastante.

## 21. ¿Para uso personal va sobrado?

Sí. Para uso personal **va sobrado** si tu objetivo es aprender, crear scripts, prototipar lógica, hacer demos de consola, experimentar con VM/opcodes, construir sistemas de eventos, probar integración con un host o montar herramientas propias.

Para uso personal lo veo especialmente fuerte en:

- Aprender cómo funciona un lenguaje por dentro.
- Hacer scripts estilo SCM/GTA.
- Prototipar lógica de gameplay.
- Crear automatizaciones simples y controladas.
- Montar demos visuales en terminal.
- Experimentar con memoria segura, bytecode y runtime.
- Usarlo como DSL embebido dentro de una app C/C++.

Mi conclusión: **SCML ya es suficientemente potente para uso personal avanzado**. La complejidad posible está más limitada por disciplina, documentación y tooling que por la VM base. Si sigues una arquitectura modular con `.scmlh`, eventos, funciones pequeñas y tests, puedes construir cosas bastante serias.

## 22. Ruta de aprendizaje recomendada

1. Lee y ejecuta `examples/helloworld.scml`.
2. Estudia `examples/variables.scml` y `examples/complexlogic.scml`.
3. Usa `std.scmlh` y reescribe ejemplos con macros.
4. Practica arrays con `examples/dynamic_arrays.scml`.
5. Estudia eventos y funciones en ejemplos complejos.
6. Ejecuta `make test` antes de tocar features.
7. Lee `stscm/std.scmlh` por bloques: base, memoria, strings, tipos, async.
8. Explora `examples/scml_project_program_complex.scml` como plantilla de proyecto serio.
9. Si quieres host embedding, compila `make cpp-example`.
10. Para apps personales, crea una carpeta propia con `main.scml` + headers `.scmlh`.

## 23. Checklist de calidad para tus scripts

Antes de considerar un script listo:

- Compila sin warnings/errores.
- Se ejecuta sin errores de runtime.
- Usa `--trace` al menos una vez.
- Libera heap/arrays que ya no usa.
- Tiene labels claros.
- Las funciones terminan con `RETURN()`.
- Los event handlers terminan con `END_THREAD()`.
- Los includes están ordenados.
- Las macros no reutilizan labels internos de forma peligrosa en el mismo scope.
- Tiene un comando reproducible de compilación/ejecución.

## 24. Glosario rápido

- **Opcode**: instrucción ejecutable de la VM.
- **SCM code**: número estilo `0004`, `00D6`, `0B14`.
- **Label**: destino de salto o llamada, por ejemplo `:MAIN`.
- **Label ref**: referencia a label, por ejemplo `@MAIN`.
- **Macro**: plantilla que expande a SCML.
- **Header `.scmlh`**: archivo de includes/macros/constantes.
- **Bytecode `.scmlbin`**: salida compilada que ejecuta la VM.
- **Heap handle**: entero que referencia memoria segura de la VM.
- **Native call**: función externa registrada por host/runtime.
- **Event handler**: label asociado a un evento.


---

# Parte II: Curso intensivo para crear sistemas complejos sin aprender sintaxis innecesaria

Esta segunda parte convierte lo anterior en un **curso completo orientado a producción personal**. No intenta cubrir cada macro existente ni cada superficie experimental del lenguaje; cubre lo que necesitas para poder construir scripts grandes, depurables y extensibles en SCML.

## 25. Mapa de dominio mínimo: lo que sí debes dominar

Para hacer cosas complejas en SCML necesitas dominar solo siete bloques:

1. **Estado**: variables `$GLOBAL`, temporales `0@`, arrays y handles de heap.
2. **Flujo**: labels, `JUMP`, condicionales y loops explícitos.
3. **Subrutinas**: `CALL`/`RETURN` con contrato de entradas y salidas.
4. **Eventos**: `BIND_EVENT`/`TRIGGER_EVENT` para desacoplar sistemas.
5. **Memoria segura**: `ARRAY_NEW`, `ARRAY_GET`, `ARRAY_SET`, `HEAP_FREE`.
6. **Interoperabilidad**: `CALL_NATIVE`, runtime modules y FFI cuando haga falta.
7. **Disciplina de proyecto**: headers `.scmlh`, nombres consistentes, trazas y pruebas.

Todo lo demás es opcional hasta que un proyecto real lo pida.

## 26. Setup de aprendizaje recomendado

Crea una carpeta de práctica fuera de `examples/` o usa un subdirectorio temporal:

```text
curso_scml/
  00_hello.scml
  01_estado.scml
  02_funciones.scml
  03_memoria.scml
  04_eventos.scml
  05_app_completa.scml
  lib.scmlh
```

Comandos de ciclo corto:

```bash
make
bin/scml compile curso_scml/00_hello.scml curso_scml/00_hello.scmlbin
bin/scml run curso_scml/00_hello.scmlbin
bin/scml run curso_scml/00_hello.scmlbin --trace
```

La regla de aprendizaje es simple: cada archivo debe compilar, ejecutarse y tener una versión trazada antes de pasar al siguiente.

## 27. Lección 1: leer SCML como bytecode humano

SCML se lee mejor como una lista de instrucciones con destino explícito:

```scml
:MAIN
0004: $A 10       ; $A = 10
0004: $B 32       ; $B = 32
0006: $SUM $A $B  ; $SUM = $A + $B
03E5: $SUM        ; print($SUM)
0001:             ; halt
```

Traducción mental:

```text
inicio -> asignar -> asignar -> calcular -> imprimir -> terminar
```

Ejercicio obligatorio:

1. Cambia `$A` y `$B`.
2. Sustituye `0006` por `0007`, `0008` y `0009`.
3. Ejecuta con `--trace` y comprueba el orden exacto de instrucciones.

## 28. Lección 2: variables con intención

Usa nombres distintos según el alcance:

| Patrón | Uso recomendado |
|---|---|
| `$APP_STATE` | Estado global de la aplicación. |
| `$PLAYER_HP` | Estado compartido de dominio. |
| `$RET` / `$STATUS` | Salida convencional de funciones. |
| `0@`, `1@`, `2@` | Temporales cortos dentro de una rutina. |
| `$ARG0`, `$ARG1` | Entradas convencionales de una función o método macro. |

Regla práctica: si una variable vive más de una función, usa `$NOMBRE_CLARO`; si solo existe para tres instrucciones, usa temporal.

Ejemplo:

```scml
#include "../std.scmlh"

:MAIN
SET($PLAYER_HP, 100)
SET($DAMAGE, 35)
CALL(@APPLY_DAMAGE)
PRINT($PLAYER_HP)
HALT()

:APPLY_DAMAGE
SUB($PLAYER_HP, $PLAYER_HP, $DAMAGE)
RETURN()
```

## 29. Lección 3: condicionales sin perderte

En SCML una condición normalmente significa: **si se cumple, salta**.

```scml
#include "../std.scmlh"

:MAIN
SET($HP, 20)
IF_GT($HP, 0, @ALIVE)
PRINT("derrotado")
HALT()

:ALIVE
PRINT("vivo")
HALT()
```

Patrón recomendado para `if/else`:

```scml
IF_GT($HP, 0, @IF_ALIVE)
JUMP(@IF_DEAD)

:IF_ALIVE
PRINT("vivo")
JUMP(@IF_END)

:IF_DEAD
PRINT("derrotado")

:IF_END
HALT()
```

No intentes escribir lógica compacta al principio. En SCML complejo gana la claridad del grafo de labels.

## 30. Lección 4: loops robustos

Loop base:

```scml
#include "../std.scmlh"

:MAIN
SET($I, 0)
SET($MAX, 5)

:LOOP_CHECK
IF_LT($I, $MAX, @LOOP_BODY)
JUMP(@LOOP_END)

:LOOP_BODY
PRINT($I)
ADD($I, $I, 1)
JUMP(@LOOP_CHECK)

:LOOP_END
HALT()
```

Checklist de todo loop:

- Variable inicializada antes del check.
- Label de check separado del body.
- Actualización garantizada.
- Label de salida explícito.
- Nada de `HALT` dentro del body salvo que sea intencional.

## 31. Lección 5: funciones con contrato

SCML no te obliga a declarar firmas. Tú debes documentarlas con comentarios:

```scml
; APPLY_DAMAGE
; in:  $PLAYER_HP, $DAMAGE
; out: $PLAYER_HP, $STATUS
; err: $STATUS = SCML_ERR_INVALID_ARG si el daño es negativo
:APPLY_DAMAGE
IF_LT($DAMAGE, 0, @APPLY_DAMAGE_BAD)
SUB($PLAYER_HP, $PLAYER_HP, $DAMAGE)
SET($STATUS, SCML_OK)
RETURN()

:APPLY_DAMAGE_BAD
SET($STATUS, SCML_ERR_INVALID_ARG)
RETURN()
```

Reglas:

1. Una función debe hacer una cosa.
2. Una función debe terminar con `RETURN()`.
3. Toda salida debe estar documentada.
4. Si una función puede fallar, escribe `$STATUS`.
5. No uses `HALT()` dentro de funciones reutilizables.

## 32. Lección 6: arrays como estructuras

Un array puede representar una entidad si reservas slots fijos:

```text
PLAYER[0] = hp
PLAYER[1] = armor
PLAYER[2] = score
PLAYER[3] = state
```

Ejemplo:

```scml
#include "../std.scmlh"

#define PLAYER_HP 0
#define PLAYER_ARMOR 1
#define PLAYER_SCORE 2
#define PLAYER_STATE 3

:MAIN
ARRAY_NEW(4, $PLAYER)
ARRAY_SET($PLAYER, PLAYER_HP, 100)
ARRAY_SET($PLAYER, PLAYER_ARMOR, 50)
ARRAY_SET($PLAYER, PLAYER_SCORE, 0)
ARRAY_SET($PLAYER, PLAYER_STATE, 1)

ARRAY_GET($HP, $PLAYER, PLAYER_HP)
PRINT($HP)

HEAP_FREE($PLAYER)
HALT()
```

Este patrón es suficiente para construir inventarios, enemigos, misiones, jobs, diálogos y estados de UI.

## 33. Lección 7: vectores tipados cuando el tamaño crece

Cuando necesites colecciones dinámicas, usa los helpers de la std:

```scml
#include "../std.scmlh"

:MAIN
VECTOR_T_NEW(T_I32, 8, $ITEMS)
VECTOR_T_PUSH(T_I32, $ITEMS, 101, @TYPE_FAIL)
VECTOR_T_PUSH(T_I32, $ITEMS, 205, @TYPE_FAIL)
VECTOR_T_GET(T_I32, $ITEM, $ITEMS, 1, @TYPE_FAIL)
PRINT($ITEM)
HEAP_FREE($ITEMS)
HALT()

:TYPE_FAIL
PRINT("tipo incorrecto")
HEAP_FREE($ITEMS)
HALT()
```

Úsalo cuando el dato sea homogéneo. Si mezclas tipos arbitrarios, vuelve a arrays con slots documentados.

## 34. Lección 8: eventos para sistemas desacoplados

Arquitectura mínima de juego/app:

```text
MAIN
  carga estado
  registra handlers
  dispara ON_START
  termina o entra en loop

ON_START
  prepara subsistemas

ON_TICK
  actualiza gameplay/simulación

ON_RENDER
  dibuja consola/UI
```

Ejemplo:

```scml
#include "../std.scmlh"

:MAIN
SET($TICK, 0)
BIND_EVENT("ON_START", @ON_START)
BIND_EVENT("ON_TICK", @ON_TICK)
TRIGGER_EVENT("ON_START")
TRIGGER_EVENT("ON_TICK")
HALT()

:ON_START
PRINT("boot ok")
END_THREAD()

:ON_TICK
ADD($TICK, $TICK, 1)
PRINT($TICK)
END_THREAD()
```

Regla: los handlers deben ser pequeños. Si un handler crece, mueve lógica a funciones con `CALL`.

## 35. Lección 9: errores explícitos

No ocultes fallos. Usa códigos:

```scml
#include "../std.scmlh"

:MAIN
SET($DEN, 0)
CALL(@SAFE_DIVIDE)
IF_EQ($STATUS, SCML_OK, @OK)
PRINT("error dividiendo")
HALT()

:OK
PRINT($RET)
HALT()

; in:  $NUM, $DEN
; out: $RET, $STATUS
:SAFE_DIVIDE
SET($NUM, 10)
IF_EQ($DEN, 0, @DIV_ZERO)
DIV($RET, $NUM, $DEN)
SET($STATUS, SCML_OK)
RETURN()

:DIV_ZERO
SET($RET, 0)
SET($STATUS, SCML_ERR_INVALID_ARG)
RETURN()
```

En programas complejos, cada operación peligrosa debe tener un camino de error: división, índice de array, native call, lectura de archivo, estado inválido.

## 36. Lección 10: arquitectura por headers

Cuando un script crece, divide por dominio:

```text
src/
  main.scml
  app_constants.scmlh
  app_state.scmlh
  inventory.scmlh
  missions.scmlh
  render.scmlh
```

Qué poner en cada header:

- `app_constants.scmlh`: `#define`, IDs, tamaños, estados.
- `app_state.scmlh`: macros para crear/liberar estado principal.
- `inventory.scmlh`: slots y funciones de inventario.
- `missions.scmlh`: eventos y estado de misiones.
- `render.scmlh`: helpers de salida, colores, pantalla.

No metas todo en `std.scmlh`; crea una std de tu proyecto.

## 37. Patrón profesional: máquina de estados

Las máquinas de estado son la forma más limpia de construir apps complejas en SCML:

```scml
#include "../std.scmlh"

#define STATE_BOOT 0
#define STATE_MENU 1
#define STATE_RUNNING 2
#define STATE_EXIT 3

:MAIN
SET($STATE, STATE_BOOT)

:APP_LOOP
IF_EQ($STATE, STATE_BOOT, @STATE_BOOT_HANDLER)
IF_EQ($STATE, STATE_MENU, @STATE_MENU_HANDLER)
IF_EQ($STATE, STATE_RUNNING, @STATE_RUNNING_HANDLER)
IF_EQ($STATE, STATE_EXIT, @STATE_EXIT_HANDLER)
PRINT("estado desconocido")
HALT()

:STATE_BOOT_HANDLER
PRINT("boot")
SET($STATE, STATE_MENU)
JUMP(@APP_LOOP)

:STATE_MENU_HANDLER
PRINT("menu")
SET($STATE, STATE_RUNNING)
JUMP(@APP_LOOP)

:STATE_RUNNING_HANDLER
PRINT("running")
SET($STATE, STATE_EXIT)
JUMP(@APP_LOOP)

:STATE_EXIT_HANDLER
PRINT("exit")
HALT()
```

Este patrón escala mejor que una cadena desordenada de saltos.

## 38. Patrón profesional: entidad con slots

Define cada entidad como array + constantes:

```scml
#define ENEMY_HP 0
#define ENEMY_X 1
#define ENEMY_Y 2
#define ENEMY_STATE 3
#define ENEMY_SIZE 4

:ENEMY_CREATE
ARRAY_NEW(ENEMY_SIZE, $RET)
ARRAY_SET($RET, ENEMY_HP, 100)
ARRAY_SET($RET, ENEMY_X, 0)
ARRAY_SET($RET, ENEMY_Y, 0)
ARRAY_SET($RET, ENEMY_STATE, 1)
RETURN()
```

Luego crea funciones pequeñas:

```scml
:ENEMY_DAMAGE
ARRAY_GET(0@, $ENEMY, ENEMY_HP)
SUB(0@, 0@, $DAMAGE)
ARRAY_SET($ENEMY, ENEMY_HP, 0@)
RETURN()
```

Nunca uses índices mágicos en el código final. Usa constantes.

## 39. Patrón profesional: comandos internos

Para apps de consola o herramientas, modela acciones como comandos numéricos:

```scml
#define CMD_NONE 0
#define CMD_ADD_ITEM 1
#define CMD_REMOVE_ITEM 2
#define CMD_SAVE 3
#define CMD_EXIT 4
```

Despacho:

```scml
:DISPATCH_COMMAND
IF_EQ($CMD, CMD_ADD_ITEM, @CMD_ADD_ITEM_HANDLER)
IF_EQ($CMD, CMD_REMOVE_ITEM, @CMD_REMOVE_ITEM_HANDLER)
IF_EQ($CMD, CMD_SAVE, @CMD_SAVE_HANDLER)
IF_EQ($CMD, CMD_EXIT, @CMD_EXIT_HANDLER)
SET($STATUS, SCML_ERR_UNSUPPORTED)
RETURN()
```

Así puedes conectar input real después, sin reescribir la lógica central.

## 40. Capstone: mini motor de misiones

Objetivo: construir un sistema pequeño pero complejo con estado, eventos, funciones, arrays y errores.

### 40.1 Requisitos

- Estados: boot, running, completed, failed.
- Player con hp y score.
- Misión con objetivo de score.
- Evento `ON_START` para inicializar.
- Evento `ON_TICK` para simular progreso.
- Función `MISSION_CHECK_STATUS` para completar o fallar.
- Liberación de memoria antes de salir.

### 40.2 Código base

```scml
#include "../std.scmlh"

#define STATE_BOOT 0
#define STATE_RUNNING 1
#define STATE_COMPLETED 2
#define STATE_FAILED 3

#define PLAYER_HP 0
#define PLAYER_SCORE 1
#define PLAYER_SIZE 2

#define MISSION_TARGET_SCORE 0
#define MISSION_TICKS 1
#define MISSION_SIZE 2

:MAIN
SET($APP_STATE, STATE_BOOT)
BIND_EVENT("ON_START", @ON_START)
BIND_EVENT("ON_TICK", @ON_TICK)
TRIGGER_EVENT("ON_START")

:MAIN_LOOP
IF_EQ($APP_STATE, STATE_RUNNING, @MAIN_TICK)
JUMP(@MAIN_END)

:MAIN_TICK
TRIGGER_EVENT("ON_TICK")
JUMP(@MAIN_LOOP)

:MAIN_END
IF_EQ($APP_STATE, STATE_COMPLETED, @PRINT_COMPLETED)
IF_EQ($APP_STATE, STATE_FAILED, @PRINT_FAILED)
JUMP(@CLEANUP)

:PRINT_COMPLETED
PRINT("mision completada")
JUMP(@CLEANUP)

:PRINT_FAILED
PRINT("mision fallida")
JUMP(@CLEANUP)

:CLEANUP
HEAP_FREE($PLAYER)
HEAP_FREE($MISSION)
HALT()

:ON_START
ARRAY_NEW(PLAYER_SIZE, $PLAYER)
ARRAY_SET($PLAYER, PLAYER_HP, 100)
ARRAY_SET($PLAYER, PLAYER_SCORE, 0)

ARRAY_NEW(MISSION_SIZE, $MISSION)
ARRAY_SET($MISSION, MISSION_TARGET_SCORE, 3)
ARRAY_SET($MISSION, MISSION_TICKS, 0)

SET($APP_STATE, STATE_RUNNING)
END_THREAD()

:ON_TICK
ARRAY_GET(0@, $MISSION, MISSION_TICKS)
ADD(0@, 0@, 1)
ARRAY_SET($MISSION, MISSION_TICKS, 0@)

ARRAY_GET(1@, $PLAYER, PLAYER_SCORE)
ADD(1@, 1@, 1)
ARRAY_SET($PLAYER, PLAYER_SCORE, 1@)

CALL(@MISSION_CHECK_STATUS)
END_THREAD()

:MISSION_CHECK_STATUS
ARRAY_GET(2@, $PLAYER, PLAYER_HP)
IF_LE(2@, 0, @MISSION_FAIL)

ARRAY_GET(3@, $PLAYER, PLAYER_SCORE)
ARRAY_GET(4@, $MISSION, MISSION_TARGET_SCORE)
IF_GE(3@, 4@, @MISSION_COMPLETE)
RETURN()

:MISSION_COMPLETE
SET($APP_STATE, STATE_COMPLETED)
RETURN()

:MISSION_FAIL
SET($APP_STATE, STATE_FAILED)
RETURN()
```

### 40.3 Mejoras que debes implementar

1. Añade daño al player cada dos ticks.
2. Añade una segunda condición de fallo por máximo de ticks.
3. Extrae `PLAYER_CREATE`, `MISSION_CREATE`, `PLAYER_ADD_SCORE` y `MISSION_TICK`.
4. Añade `--trace` y verifica que el loop termina.
5. Crea un header `mission_system.scmlh` con constantes y macros.

Si puedes completar esto, ya tienes la base para crear sistemas bastante complejos.

## 41. Checklist para proyectos grandes

Antes de crecer un proyecto, valida esto:

- Existe un `:MAIN` pequeño.
- Hay una máquina de estados clara.
- Los eventos no contienen lógica pesada.
- Las funciones tienen comentarios `in/out/err`.
- Los arrays tienen constantes de slots.
- No hay índices mágicos.
- Cada `ARRAY_NEW`/`HEAP_ALLOC` tiene ruta de `HEAP_FREE`.
- Cada error recuperable escribe `$STATUS`.
- Los labels tienen prefijos por módulo: `PLAYER_`, `MISSION_`, `UI_`.
- Puedes ejecutar el script con `--trace` sin perderte.

## 42. Qué estudiar después, solo si lo necesitas

- Para interoperabilidad profunda: `docs/SCML_FFI_RUNTIME_GUIDE_ES.md`.
- Para módulos estándar: `docs/SCML_STD_MODULES_ES.md`.
- Para superficie avanzada tipo C++17/C++20: `docs/SCML_CPP17_SUPERSET_ES.md` y `docs/SCML_CPP20_DOMINATION_PACK_ES.md`.
- Para features SCML26: `docs/SCML26_ADVANCED_SURFACE_ES.md`.
- Para integración GTA: `GTA Integration/README.md`.

Ruta final recomendada:

```text
base opcode -> macros std -> arrays/heap -> funciones -> eventos -> máquina de estados -> native/FFI -> proyecto propio
```

No memorices todo. Aprende el núcleo, usa headers para esconder repetición y vuelve a la documentación avanzada solo cuando un problema real lo exija.
