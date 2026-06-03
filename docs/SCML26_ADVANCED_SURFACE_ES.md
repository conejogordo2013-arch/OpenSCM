# SCML26 Advanced Surface

Este paquete añade una capa estable y segura para experimentar con rasgos modernos tipo C++26 sin romper el bytecode SCML existente.

## Objetos de clase, submódulos y VM metadata

`CLASS`, `NAMESPACE`, `MODULE`, `TMPL`, `concept`, `enum`, `submodule`, `MACRO` y métodos `fn` dentro de clases se aceptan como bloques de metadatos. El compilador ahora emite registros `meta.*` al bytecode, por lo que la VM conoce automáticamente clases, namespaces, templates, macros, métodos, clases derivadas y submódulos declarados antes de `script MAIN`. Para crear un objeto de clase en runtime se usa:

```scml
useClass(Player) -> $player;
class_set_module($player, 101);
class_set_submodule($player, 202);
```

El objeto es un arreglo SCML seguro de 4 slots: tipo, nombre/hash reservado, módulo y submódulo. Además, la registry de la VM se consulta desde la superficie moderna con `getClassesInModule(...) -> out`, `getAllClassesInSubmodule(...) -> out`, `getClassName(obj) -> out`, `getClassModule(obj) -> out`, `getClassMethods(obj) -> out`, `getDerivedClasses(...) -> out`, `getNamespaceInClass(...) -> out`, `getTemplateInClass(...) -> out`, `getMacroInClass(...) -> out`, `getSubmodulesInClass(...) -> out` y `useSub(...) -> out`.

## Metaprogramación, reflexión y contratos

La superficie moderna acepta:

- `static_assert(cond, "mensaje")` con mensajes generados por el usuario cuando la condición constante falla.
- `contract_assert`, `pre` y `post` para contratos ligeros; las condiciones constantes falsas se rechazan en compilación.
- `define_static_string`, `define_static_object` y `define_static_array` para materializar datos estáticos.
- Placeholder `_` en declaraciones y structured bindings.
- Prohibición de `return &...` y `return ref(...)` para evitar referencias a temporales.
- `nameTemplate(namespace, template, class) -> out` combina namespaces y templates para nombrar clases template de forma reproducible.
- `makeArrayClass(class, rank) -> out`, `rttiVariable(...) -> out`, `classPointer(obj) -> out` y `classStringJoin(...) -> out` cubren clases tipo array, RTTI de variables, punteros sobre clases y strings compuestos.

El módulo `stscm/modules/meta.scmlh` añade helpers como `reflect_value`, `reflect_function`, `reflect_parameter`, `reflect_annotation`, `reflect_error`, consultas de clase (`get_classes_in_module`, `get_all_classes_in_submodule`, `get_class_methods`, `get_derived_classes`, `get_namespace_in_class`, `get_template_in_class`, `get_macro_in_class`), `use_sub`, `name_template`, `make_array_class`, `rtti_variable`, `class_string_join`, pattern matching (`pattern_match_i32`), `pack_index` y `trivial_relocate_safe`.

## `#embed` y `__has_embed`

El preprocesador soporta formas inline:

```scml
let $ok: i32 = __has_embed("asset.txt");
define_static_string($blob, #embed("asset.txt"));
```

`#embed` produce un literal de string escapado y `__has_embed` produce `1` o `0`.

## Nuevos módulos STD

`stscm/modules/std_modules.scmlh` importa ahora:

- `execution.scmlh`: sender/receiver, tareas, scheduler paralelo, contextos async, `when_all2`, scheduling diferido y yield explícito.
- `safety.scmlh`: hardening, debugging, hazard pointers, RCU y wrappers `safe_array_read`/`safe_array_write` con comprobación de bounds.
- `linalg.scmlh`: `simd_pack4`, `simd_add4`, `linalg_dot2`, `inplace_vector_create`, `hive_create` y helpers de pattern matching por rango.
- `text_encoding.scmlh`: nombres y wrappers para validación/transcodificación de texto.

Estos módulos son macros sobre opcodes estables (`0B14`, `0B13`, `0B12`, async y `CALL_NATIVE`) para conservar compatibilidad binaria.
