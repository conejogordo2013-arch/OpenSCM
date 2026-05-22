# SCML Advanced Tools (VSCode)

Extensión de VSCode para trabajar con SCML sin texto plano: resaltado de sintaxis, snippets y comandos para compilar/ejecutar.

## Características

- Resaltado de sintaxis para `.scml` y `.scmlh`.
- Snippets base (`mainloop`, `scmacro`, `ifdef`).
- Comandos:
  - `SCML: Compile Current File`
  - `SCML: Run Current Binary`
  - `SCML: Compile and Run Current File`
- Detección automática del ejecutable SCML en este orden:
  1. `scml.executablePath`
  2. `${workspaceFolder}/bin/scml` (o `scml.exe` en Windows)
  3. `scml` en el `PATH`

## Instalación local (modo desarrollo)

1. Abre la carpeta de la extensión en VSCode:
   - `vscode-scml-extension/`
2. Pulsa `F5` para abrir **Extension Development Host**.
3. En la nueva ventana, abre tu proyecto SCML.
4. Crea o abre un archivo `.scml` y verifica que ya tiene colores.

## Instalación como VSIX

Desde `vscode-scml-extension/`:

```bash
npm install
npx @vscode/vsce package
```

Esto genera un `.vsix`.

Luego en VSCode:

- `Extensions` → `...` → **Install from VSIX...**
- Selecciona el `.vsix` generado.

## Configuración recomendada

En `Settings` (JSON):

```json
{
  "scml.executablePath": "/ruta/absoluta/a/scml",
  "scml.defaultBinOutputDir": "${workspaceFolder}/bin",
  "scml.runWithTrace": false
}
```

## Cómo usar (paso a paso)

1. Abre un archivo `.scml`.
2. `Ctrl+Shift+P` → ejecuta `SCML: Compile Current File`.
3. Se generará un `.scmlbin` en la carpeta configurada (por defecto `bin/`).
4. `Ctrl+Shift+P` → `SCML: Run Current Binary` para ejecutarlo.
5. O usa `SCML: Compile and Run Current File` para hacer ambos pasos juntos.

## Solución de problemas

- **No aparece color de sintaxis**:
  - Comprueba que el archivo termina en `.scml` o `.scmlh`.
  - Ejecuta `Developer: Inspect Editor Tokens and Scopes` para validar el scope `source.scml`.
- **No encuentra `scml`**:
  - Define `scml.executablePath` con ruta absoluta.
  - Verifica que `bin/scml` existe y tiene permisos de ejecución.
- **Compila pero no ejecuta**:
  - Confirma que el `.scmlbin` se creó en `scml.defaultBinOutputDir`.

## Flujo recomendado para este repo

Si estás trabajando dentro de `OpenSCM`:

- Compilador esperado: `./bin/scml`
- Salida binaria: `./bin/*.scmlbin`

Con esa estructura, la extensión funciona sin configuración adicional en la mayoría de casos.
