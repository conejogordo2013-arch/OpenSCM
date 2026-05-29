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

