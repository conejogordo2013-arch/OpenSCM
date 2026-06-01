const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');
const os = require('os');

const SCML_SELECTOR = [
  { language: 'scml', scheme: 'file' },
  { language: 'scml', scheme: 'untitled' }
];

const BUILTIN_OPCODES = [
  ['0000', 'NOP', 0, 0, 'No operation.'],
  ['0001', 'HALT', 0, 0, 'Stop the VM.'],
  ['0004', 'SET', 2, 2, 'Assign a value to a variable.'],
  ['0005', 'GET', 1, 1, 'Push/evaluate a value.'],
  ['0006', 'ADD', 3, 3, 'Integer/float addition.'],
  ['0007', 'SUB', 3, 3, 'Integer/float subtraction.'],
  ['0008', 'MUL', 3, 3, 'Integer/float multiplication.'],
  ['0009', 'DIV', 3, 3, 'Integer/float division.'],
  ['000A', 'JUMP', 1, 1, 'Jump to a label.'],
  ['000B', 'WAIT', 1, 1, 'Sleep/yield through runtime.wait.'],
  ['00D6', 'IF_EQ', 3, 3, 'Jump if values are equal.'],
  ['00D7', 'IF_NE', 3, 3, 'Jump if values are different.'],
  ['00D8', 'IF_GT', 3, 3, 'Jump if left value is greater.'],
  ['00D9', 'IF_LT', 3, 3, 'Jump if left value is smaller.'],
  ['03E5', 'PRINT', 1, 1, 'Print with newline.'],
  ['03E6', 'LOG', 1, 1, 'Log with newline.'],
  ['0A30', 'BIND_EVENT', 2, 2, 'Bind an event to a label.'],
  ['0A31', 'TRIGGER_EVENT', 1, 1, 'Queue an event by name.'],
  ['0B10', 'ALLOC', 2, 2, 'Allocate heap cells.'],
  ['0B11', 'FREE', 1, 1, 'Free a heap reference.'],
  ['0B12', 'READ', 3, 3, 'Read from heap/array.'],
  ['0B13', 'WRITE', 3, 3, 'Write to heap/array.'],
  ['0B14', 'ARRAY_CREATE', 2, 2, 'Create an array.'],
  ['0B31', 'CALL_NATIVE', 1, 8, 'Call a native module/function.'],
  ['0B49', 'ASYNC_SPAWN', 2, 2, 'Spawn a cooperative async label task.'],
  ['0B4A', 'ASYNC_DONE', 2, 2, 'Check if an async task is complete.'],
  ['0B4B', 'TYPE_DECL', 2, 2, 'Declare a static type for a variable.'],
  ['0B4C', 'TYPE_ASSERT', 3, 3, 'Runtime type assertion helper.'],
  ['0D00', 'CALL', 1, 8, 'Call a SCML label or native function.'],
  ['0D01', 'RETURN', 0, 0, 'Return from a CALL.'],
  ['0D02', 'END_THREAD', 0, 0, 'End current event/task thread.']
];

const DIRECTIVES = ['#include', '#define', '#undef', '#if', '#ifdef', '#ifndef', '#elif', '#else', '#endif', '#error', '#warning', '#line', '#pragma'];
const KEYWORDS = ['macro', 'endmacro', 'CALL', 'JUMP', 'GOTO', 'RETURN', 'END_THREAD', 'WAIT', 'use', 'script', 'module', 'namespace', 'class', 'fn', 'function', 'task', 'let', 'var', 'const', 'if', 'else', 'while', 'for', 'break', 'continue', 'goto', 'call', 'spawn', 'return', 'halt', 'yield', 'print', 'log', 'wait'];
const TYPE_NAMES = ['i8', 'i16', 'i32', 'i64', 'u8', 'u16', 'u32', 'u64', 'f32', 'f64', 'str', 'bool', 'ref', 'any', 'int', 'float', 'number', 'string', 'array<any>'];

let diagnostics;
let outputChannel;
let cachedCatalog;

function substituteVariables(value, vars) {
  return String(value || '').replace(/\$\{(workspaceFolder|file|fileDirname|fileBasename|fileBasenameNoExtension)\}/g, (_, key) => vars[key] || '');
}

function resolveScmlExecutable(workspaceFolder) {
  const cfg = vscode.workspace.getConfiguration('scml');
  const configured = cfg.get('executablePath', '').trim();
  const root = workspaceFolder ? workspaceFolder.uri.fsPath : '';
  if (configured) {
    const expanded = substituteVariables(configured, { workspaceFolder: root });
    if (path.isAbsolute(expanded) || expanded.includes(path.sep)) return expanded;
    return expanded;
  }
  const candidates = [];
  if (workspaceFolder) {
    candidates.push(path.join(workspaceFolder.uri.fsPath, 'bin', process.platform === 'win32' ? 'scml.exe' : 'scml'));
    candidates.push(path.join(workspaceFolder.uri.fsPath, process.platform === 'win32' ? 'scml.exe' : 'scml'));
  }
  for (const c of candidates) if (fs.existsSync(c)) return c;
  return process.platform === 'win32' ? 'scml.exe' : 'scml';
}

function quoteForLog(value) {
  const s = String(value);
  return /[\s"]/.test(s) ? JSON.stringify(s) : s;
}

function commandLineForLog(cmd, args) {
  return [cmd, ...args].map(quoteForLog).join(' ');
}

function runCommand(cmd, args, cwd, options = {}) {
  const silent = Boolean(options.silent);
  if (outputChannel && !silent) outputChannel.appendLine(`$ ${commandLineForLog(cmd, args)}`);
  return new Promise((resolve, reject) => {
    const child = cp.spawn(cmd, args, { cwd, shell: false });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => stdout += d.toString());
    child.stderr.on('data', d => stderr += d.toString());
    child.on('error', err => reject(new Error(`Unable to start '${cmd}'. Configure scml.executablePath. ${err.message}`)));
    child.on('close', code => {
      if (outputChannel && !silent) {
        if (stdout) outputChannel.append(stdout);
        if (stderr) outputChannel.append(stderr);
      }
      return code === 0 ? resolve({ stdout, stderr }) : reject(new Error(stderr || stdout || `Exit code ${code}`));
    });
  });
}

function getActiveScmlFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) throw new Error('No active editor.');
  const file = editor.document.uri.fsPath;
  if (!file.endsWith('.scml') && !file.endsWith('.scmlh')) throw new Error('Active file is not .scml/.scmlh');
  return { file, workspaceFolder: vscode.workspace.getWorkspaceFolder(editor.document.uri) };
}

function extensionRepoRoot() {
  return path.resolve(__dirname, '..');
}

function stripComment(line) {
  let inString = false;
  let escaped = false;
  for (let i = 0; i < line.length; i++) {
    const ch = line[i];
    if (escaped) { escaped = false; continue; }
    if (inString && ch === '\\') { escaped = true; continue; }
    if (ch === '"') inString = !inString;
    if (!inString && ch === ';') return line.slice(0, i);
    if (!inString && ch === '/' && line[i + 1] === '/') return line.slice(0, i);
  }
  return line;
}

function rangeForMatch(document, lineNo, match, groupIndex = 0) {
  const text = document.lineAt(lineNo).text;
  const token = match[groupIndex];
  const start = text.indexOf(token, match.index);
  return new vscode.Range(lineNo, start, lineNo, start + token.length);
}

function readTextIfExists(file) {
  try { return fs.existsSync(file) ? fs.readFileSync(file, 'utf8') : ''; } catch (_) { return ''; }
}

function parseOpcodeCatalog() {
  const catalog = new Map();
  for (const [code, name, min, max, detail] of BUILTIN_OPCODES) catalog.set(name, { code, name, min, max, detail });
  const opFile = path.join(extensionRepoRoot(), 'opcode', 'opcode.c');
  const text = readTextIfExists(opFile);
  const re = /\{0x([0-9A-Fa-f]{4}),\s*SCML_OP_[A-Z0-9_]+,\s*"([A-Z0-9_]+)",\s*(\d+),\s*(\d+)\}/g;
  let m;
  while ((m = re.exec(text))) {
    const [, code, name, min, max] = m;
    catalog.set(name, { code: code.toUpperCase(), name, min: Number(min), max: Number(max), detail: `SCML opcode 0x${code.toUpperCase()} (${min}..${max} args)` });
  }
  return catalog;
}

function parseMacroCatalog() {
  const catalog = new Map();
  const files = [path.join(extensionRepoRoot(), 'stscm', 'std.scmlh'), path.join(extensionRepoRoot(), 'std.scmlh')];
  const re = /^\s*macro\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*[:{]/gm;
  for (const file of files) {
    const text = readTextIfExists(file);
    let m;
    while ((m = re.exec(text))) {
      const args = m[2].trim() ? m[2].split(',').map(a => a.trim()) : [];
      catalog.set(m[1], { name: m[1], args, file });
    }
  }
  return catalog;
}

function getCatalog() {
  if (!cachedCatalog) cachedCatalog = { opcodes: parseOpcodeCatalog(), macros: parseMacroCatalog() };
  return cachedCatalog;
}

function wordAt(document, position) {
  const range = document.getWordRangeAtPosition(position, /[@:$#]?[A-Za-z_][A-Za-z0-9_@.$-]*|[0-9A-Fa-f]{4}/);
  return range ? { text: document.getText(range), range } : undefined;
}

function parseDocument(document) {
  const labels = new Map();
  const labelRefs = [];
  const macros = new Map();
  const macroEnds = [];
  const defines = new Map();
  const variables = new Map();
  const includes = [];
  const diagnosticsList = [];
  const typeDecls = new Map();
  const catalog = getCatalog();
  let inMacro = null;

  for (let lineNo = 0; lineNo < document.lineCount; lineNo++) {
    const raw = document.lineAt(lineNo).text;
    const line = stripComment(raw);
    const trimmed = line.trim();
    if (!trimmed) continue;

    const include = trimmed.match(/^#\s*include\s+["<]([^">]+)[">]/) || trimmed.match(/^use\s+["<]([^">]+)[">]/);
    if (include) includes.push({ path: include[1], range: rangeForMatch(document, lineNo, include, 1) });

    const define = trimmed.match(/^#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)/);
    if (define) defines.set(define[1], new vscode.Location(document.uri, rangeForMatch(document, lineNo, define, 1)));

    const macro = trimmed.match(/^macro\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)\s*[:{]/);
    if (macro) {
      inMacro = { name: macro[1], line: lineNo };
      macros.set(macro[1], { name: macro[1], args: macro[2].trim() ? macro[2].split(',').map(a => a.trim()) : [], location: new vscode.Location(document.uri, rangeForMatch(document, lineNo, macro, 1)), endLine: lineNo });
      continue;
    }
    if (/^endmacro\b/.test(trimmed)) {
      if (inMacro && macros.has(inMacro.name)) macros.get(inMacro.name).endLine = lineNo;
      macroEnds.push(lineNo);
      inMacro = null;
      continue;
    }

    const label = trimmed.match(/^:([A-Za-z_][A-Za-z0-9_]*)\b/);
    if (label) {
      const location = new vscode.Location(document.uri, rangeForMatch(document, lineNo, label, 1));
      if (labels.has(label[1])) diagnosticsList.push(new vscode.Diagnostic(location.range, `Duplicate label :${label[1]}`, vscode.DiagnosticSeverity.Error));
      labels.set(label[1], location);
      continue;
    }

    for (const ref of line.matchAll(/@([A-Za-z_][A-Za-z0-9_]*)\b/g)) {
      labelRefs.push({ name: ref[1], range: rangeForMatch(document, lineNo, ref, 1) });
    }
    for (const varMatch of line.matchAll(/[$]?[A-Za-z_][A-Za-z0-9_]*@?\b/g)) {
      const name = varMatch[0];
      if (KEYWORDS.includes(name) || DIRECTIVES.includes(name) || catalog.opcodes.has(name) || catalog.macros.has(name)) continue;
      if (name.startsWith('$') || name.endsWith('@')) variables.set(name, new vscode.Location(document.uri, rangeForMatch(document, lineNo, varMatch, 0)));
    }

    const opToken = trimmed.match(/^([A-Za-z_][A-Za-z0-9_.]*|[0-9A-Fa-f]{4})\s*:/) || trimmed.match(/^([A-Za-z_][A-Za-z0-9_.]*)\s*\(/) || trimmed.match(/^([A-Za-z_][A-Za-z0-9_.]*)\b/);
    if (opToken && !trimmed.startsWith('#') && !/^[$]?[A-Za-z_][A-Za-z0-9_@]*\s*(=|[+\-*/%&|^]=|<<=|>>=|\+\+|--)/.test(trimmed)) {
      const name = opToken[1].toUpperCase();
      const rawName = opToken[1];
      if (/^[0-9A-Fa-f]{4}$/.test(rawName)) {
        const exists = [...catalog.opcodes.values()].some(op => op.code === rawName.toUpperCase());
        if (!exists) diagnosticsList.push(new vscode.Diagnostic(rangeForMatch(document, lineNo, opToken, 1), `Unknown SCML numeric opcode ${rawName}`, vscode.DiagnosticSeverity.Warning));
      } else if (!catalog.opcodes.has(name) && !catalog.macros.has(rawName) && !KEYWORDS.includes(rawName) && !rawName.includes('.')) {
        diagnosticsList.push(new vscode.Diagnostic(rangeForMatch(document, lineNo, opToken, 1), `Unknown SCML opcode or macro '${rawName}'`, vscode.DiagnosticSeverity.Warning));
      }
    }

    const typeDecl = trimmed.match(/^(?:TYPE_DECL|0B4B:)\s+([$]?[A-Za-z_][A-Za-z0-9_@]*)\s+"([^"]+)"/);
    const letDecl = trimmed.match(/^LET_(I32|F32|STR|ANY)\s*\(\s*([$]?[A-Za-z_][A-Za-z0-9_@]*)/);
    if (typeDecl) typeDecls.set(typeDecl[1], { type: typeDecl[2], line: lineNo });
    if (letDecl) typeDecls.set(letDecl[2], { type: letDecl[1].toLowerCase(), line: lineNo });
  }

  for (const ref of labelRefs) {
    if (!labels.has(ref.name)) diagnosticsList.push(new vscode.Diagnostic(ref.range, `Unresolved label @${ref.name}`, vscode.DiagnosticSeverity.Error));
  }
  for (const inc of includes) {
    if (document.uri.scheme !== 'file') continue;
    const base = path.dirname(document.uri.fsPath);
    const resolved = path.resolve(base, inc.path);
    if (!fs.existsSync(resolved)) diagnosticsList.push(new vscode.Diagnostic(inc.range, `Included file not found: ${inc.path}`, vscode.DiagnosticSeverity.Error));
  }

  return { labels, labelRefs, macros, macroEnds, defines, variables, includes, diagnostics: diagnosticsList, typeDecls };
}

function refreshDiagnostics(document) {
  if (!diagnostics || (document.languageId !== 'scml' && !document.fileName.endsWith('.scmlh'))) return;
  const cfg = vscode.workspace.getConfiguration('scml', document.uri);
  if (!cfg.get('diagnostics.enabled', true)) { diagnostics.set(document.uri, []); return; }
  const parsed = parseDocument(document);
  diagnostics.set(document.uri, parsed.diagnostics);
  if (cfg.get('diagnostics.compileOnSave', true) && !document.isDirty && document.uri.scheme === 'file' && document.fileName.endsWith('.scml')) {
    runCompilerDiagnostics(document).catch(() => undefined);
  }
}

async function runCompilerDiagnostics(document) {
  const workspaceFolder = vscode.workspace.getWorkspaceFolder(document.uri);
  const exe = resolveScmlExecutable(workspaceFolder);
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(document.uri.fsPath);
  const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'scml-diag-'));
  const outBin = path.join(tmpDir, 'diagnostic.scmlbin');
  try {
    await runCommand(exe, ['compile', document.uri.fsPath, outBin], cwd, { silent: true });
  } catch (e) {
    const message = String(e.message || e);
    const m = message.match(/line\s+(\d+):\s*(.*)/i);
    const line = m ? Math.max(0, Number(m[1]) - 1) : 0;
    const text = m ? m[2] : message.trim();
    const existing = diagnostics.get(document.uri) || [];
    const range = document.lineAt(Math.min(line, document.lineCount - 1)).range;
    diagnostics.set(document.uri, existing.concat(new vscode.Diagnostic(range, `Compiler: ${text}`, vscode.DiagnosticSeverity.Error)));
  } finally {
    try { fs.rmSync(tmpDir, { recursive: true, force: true }); } catch (_) {}
  }
}

function configArray(cfg, key) {
  const value = cfg.get(key, []);
  return Array.isArray(value) ? value.map(String).filter(Boolean) : [];
}

function outputBinFor(file, workspaceFolder) {
  const cfg = vscode.workspace.getConfiguration('scml');
  const root = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
  const vars = {
    workspaceFolder: root,
    file,
    fileDirname: path.dirname(file),
    fileBasename: path.basename(file),
    fileBasenameNoExtension: path.basename(file).replace(/\.scmlh?$/, '')
  };
  const outDir = substituteVariables(cfg.get('defaultBinOutputDir', '${workspaceFolder}/bin'), vars);
  return path.join(outDir, `${vars.fileBasenameNoExtension}.scmlbin`);
}

function manifestPathFor(workspaceFolder) {
  const root = workspaceFolder ? workspaceFolder.uri.fsPath : (vscode.workspace.workspaceFolders?.[0]?.uri.fsPath || process.cwd());
  const cfg = vscode.workspace.getConfiguration('scml');
  return substituteVariables(cfg.get('manifestPath', '${workspaceFolder}/scml.pkg'), { workspaceFolder: root });
}

function prepareOutput() {
  const cfg = vscode.workspace.getConfiguration('scml');
  if (cfg.get('clearOutputBeforeTask', true)) outputChannel.clear();
  outputChannel.show(true);
}

async function compileCurrent() {
  const { file, workspaceFolder } = getActiveScmlFile();
  if (file.endsWith('.scmlh')) throw new Error('Header files (.scmlh) cannot be compiled directly. Open a .scml source or use SCML: Check Current File.');
  prepareOutput();
  const exe = resolveScmlExecutable(workspaceFolder);
  const cfg = vscode.workspace.getConfiguration('scml');
  const outBin = outputBinFor(file, workspaceFolder);
  fs.mkdirSync(path.dirname(outBin), { recursive: true });
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
  const result = await runCommand(exe, ['compile', ...configArray(cfg, 'additionalCompileArgs'), file, outBin], cwd);
  if (result.stderr) vscode.window.showWarningMessage(result.stderr.trim());
  vscode.window.showInformationMessage(`SCML compiled: ${outBin}`);
  return outBin;
}

async function checkCurrentFile() {
  const { file, workspaceFolder } = getActiveScmlFile();
  prepareOutput();
  const exe = resolveScmlExecutable(workspaceFolder);
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
  await runCommand(exe, ['check', file], cwd);
  vscode.window.showInformationMessage('SCML check completed.');
}

async function runCurrentBinary(binPath) {
  const { workspaceFolder } = getActiveScmlFile();
  const exe = resolveScmlExecutable(workspaceFolder);
  const cfg = vscode.workspace.getConfiguration('scml');
  const args = ['run', binPath];
  if (cfg.get('runWithTrace', false)) args.push('--trace');
  args.push(...configArray(cfg, 'additionalRunArgs'));
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(binPath);
  if (cfg.get('runInTerminal', false)) {
    const terminal = vscode.window.createTerminal({ name: 'SCML Run', cwd });
    terminal.show(true);
    terminal.sendText(commandLineForLog(exe, args));
    return;
  }
  prepareOutput();
  await runCommand(exe, args, cwd);
  vscode.window.showInformationMessage('SCML run completed. Check Output panel.');
}

async function buildProject(showMetadata = false) {
  const editor = vscode.window.activeTextEditor;
  const workspaceFolder = editor ? vscode.workspace.getWorkspaceFolder(editor.document.uri) : vscode.workspace.workspaceFolders?.[0];
  prepareOutput();
  const exe = resolveScmlExecutable(workspaceFolder);
  const manifest = manifestPathFor(workspaceFolder);
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(manifest);
  await runCommand(exe, [showMetadata ? 'metadata' : 'build', manifest], cwd);
  vscode.window.showInformationMessage(showMetadata ? 'SCML metadata completed.' : 'SCML project build completed.');
}

function opcodeCompletionItems() {
  const { opcodes } = getCatalog();
  return [...opcodes.values()].flatMap(op => {
    const args = Array.from({ length: op.min }, (_, i) => `\${${i + 1}:arg${i + 1}}`).join(' ');
    const named = new vscode.CompletionItem(op.name, vscode.CompletionItemKind.Function);
    named.detail = `${op.name} (${op.min}..${op.max} args) opcode 0x${op.code}`;
    named.documentation = new vscode.MarkdownString(op.detail || `SCML opcode 0x${op.code}.`);
    named.insertText = new vscode.SnippetString(`${op.name}${args ? ' ' + args : ''}`);
    const numeric = new vscode.CompletionItem(`${op.code}:`, vscode.CompletionItemKind.Operator);
    numeric.detail = `${op.name} (${op.min}..${op.max} args)`;
    numeric.documentation = named.documentation;
    numeric.insertText = new vscode.SnippetString(`${op.code}:${args ? ' ' + args : ''}`);
    return [named, numeric];
  });
}

function macroCompletionItems() {
  const { macros } = getCatalog();
  return [...macros.values()].map(macro => {
    const item = new vscode.CompletionItem(macro.name, vscode.CompletionItemKind.Snippet);
    item.detail = `SCML macro (${macro.args.join(', ')})`;
    item.documentation = new vscode.MarkdownString(`Expands standard macro \`${macro.name}\`.`);
    const args = macro.args.map((arg, i) => `\${${i + 1}:${arg}}`).join(', ');
    item.insertText = new vscode.SnippetString(`${macro.name}(${args})`);
    return item;
  });
}

function activate(context) {
  outputChannel = vscode.window.createOutputChannel('SCML');
  diagnostics = vscode.languages.createDiagnosticCollection('scml');
  context.subscriptions.push(outputChannel, diagnostics);

  context.subscriptions.push(vscode.commands.registerCommand('scml.compile', async () => {
    try { await compileCurrent(); } catch (e) { vscode.window.showErrorMessage(`SCML compile failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.run', async () => {
    try {
      const { file, workspaceFolder } = getActiveScmlFile();
      await runCurrentBinary(outputBinFor(file, workspaceFolder));
    } catch (e) { vscode.window.showErrorMessage(`SCML run failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.compileAndRun', async () => {
    try { const bin = await compileCurrent(); await runCurrentBinary(bin); } catch (e) { vscode.window.showErrorMessage(`SCML compile+run failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.checkCurrentFile', async () => {
    try { await checkCurrentFile(); } catch (e) { vscode.window.showErrorMessage(`SCML check failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.buildProject', async () => {
    try { await buildProject(false); } catch (e) { vscode.window.showErrorMessage(`SCML project build failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.showMetadata', async () => {
    try { await buildProject(true); } catch (e) { vscode.window.showErrorMessage(`SCML metadata failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.revealExecutable', () => {
    const editor = vscode.window.activeTextEditor;
    const folder = editor ? vscode.workspace.getWorkspaceFolder(editor.document.uri) : vscode.workspace.workspaceFolders?.[0];
    vscode.window.showInformationMessage(`SCML executable: ${resolveScmlExecutable(folder)}`);
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.selectExecutable', async () => {
    const picked = await vscode.window.showOpenDialog({
      canSelectFiles: true,
      canSelectFolders: false,
      canSelectMany: false,
      title: 'Select the SCML CLI executable that supports modern syntax'
    });
    if (!picked || !picked[0]) return;
    await vscode.workspace.getConfiguration('scml').update('executablePath', picked[0].fsPath, vscode.ConfigurationTarget.Workspace);
    cachedCatalog = undefined;
    vscode.window.showInformationMessage(`SCML executable selected: ${picked[0].fsPath}`);
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.restartLanguageServices', () => {
    cachedCatalog = undefined;
    for (const doc of vscode.workspace.textDocuments) refreshDiagnostics(doc);
    vscode.window.showInformationMessage('SCML language services restarted.');
  }));

  context.subscriptions.push(vscode.languages.registerCompletionItemProvider(SCML_SELECTOR, {
    provideCompletionItems(document, position) {
      const parsed = parseDocument(document);
      const labels = [...parsed.labels.keys()].map(name => {
        const item = new vscode.CompletionItem(`@${name}`, vscode.CompletionItemKind.Reference);
        item.detail = 'SCML label reference';
        return item;
      });
      const directives = DIRECTIVES.map(d => new vscode.CompletionItem(d, vscode.CompletionItemKind.Keyword));
      const types = TYPE_NAMES.map(t => new vscode.CompletionItem(`"${t}"`, vscode.CompletionItemKind.TypeParameter));
      return [...opcodeCompletionItems(), ...macroCompletionItems(), ...labels, ...directives, ...types];
    }
  }, '@', '#', '"'));

  context.subscriptions.push(vscode.languages.registerHoverProvider(SCML_SELECTOR, {
    provideHover(document, position) {
      const w = wordAt(document, position);
      if (!w) return undefined;
      const parsed = parseDocument(document);
      const { opcodes, macros } = getCatalog();
      const token = w.text.replace(/^@/, '');
      const upper = token.toUpperCase();
      if (parsed.labels.has(token)) return new vscode.Hover(new vscode.MarkdownString(`SCML label \`:${token}\``), w.range);
      if (opcodes.has(upper)) {
        const op = opcodes.get(upper);
        return new vscode.Hover(new vscode.MarkdownString(`**${op.name}** / \`${op.code}:\`\n\nArguments: ${op.min}..${op.max}\n\n${op.detail || ''}`), w.range);
      }
      if (macros.has(token)) {
        const macro = macros.get(token);
        return new vscode.Hover(new vscode.MarkdownString(`**macro ${macro.name}(${macro.args.join(', ')})**`), w.range);
      }
      if (TYPE_NAMES.includes(token.replace(/"/g, ''))) return new vscode.Hover(new vscode.MarkdownString(`SCML static type \`${token}\``), w.range);
      return undefined;
    }
  }));

  context.subscriptions.push(vscode.languages.registerDefinitionProvider(SCML_SELECTOR, {
    provideDefinition(document, position) {
      const w = wordAt(document, position);
      if (!w) return undefined;
      const parsed = parseDocument(document);
      const name = w.text.replace(/^[@:]/, '');
      if (parsed.labels.has(name)) return parsed.labels.get(name);
      if (parsed.macros.has(name)) return parsed.macros.get(name).location;
      if (parsed.defines.has(name)) return parsed.defines.get(name);
      if (parsed.variables.has(w.text)) return parsed.variables.get(w.text);
      return undefined;
    }
  }));

  context.subscriptions.push(vscode.languages.registerReferenceProvider(SCML_SELECTOR, {
    provideReferences(document, position) {
      const w = wordAt(document, position);
      if (!w) return [];
      const name = w.text.replace(/^[@:]/, '');
      const locations = [];
      for (let i = 0; i < document.lineCount; i++) {
        const line = document.lineAt(i).text;
        const re = new RegExp(`[@:]?\\b${name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`, 'g');
        let m;
        while ((m = re.exec(line))) locations.push(new vscode.Location(document.uri, new vscode.Range(i, m.index, i, m.index + m[0].length)));
      }
      return locations;
    }
  }));

  context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(SCML_SELECTOR, {
    provideDocumentSymbols(document) {
      const parsed = parseDocument(document);
      const symbols = [];
      for (const [name, loc] of parsed.labels) symbols.push(new vscode.DocumentSymbol(`:${name}`, 'SCML label', vscode.SymbolKind.Function, loc.range, loc.range));
      for (const [name, macro] of parsed.macros) {
        const range = new vscode.Range(macro.location.range.start.line, 0, macro.endLine, document.lineAt(macro.endLine).text.length);
        symbols.push(new vscode.DocumentSymbol(`macro ${name}`, macro.args.join(', '), vscode.SymbolKind.Method, range, macro.location.range));
      }
      return symbols;
    }
  }));

  context.subscriptions.push(vscode.languages.registerFoldingRangeProvider(SCML_SELECTOR, {
    provideFoldingRanges(document) {
      const ranges = [];
      let currentLabel = null;
      for (let i = 0; i < document.lineCount; i++) {
        const trimmed = document.lineAt(i).text.trim();
        if (/^:[A-Za-z_][A-Za-z0-9_]*/.test(trimmed)) {
          if (currentLabel !== null && i - currentLabel > 1) ranges.push(new vscode.FoldingRange(currentLabel, i - 1, vscode.FoldingRangeKind.Region));
          currentLabel = i;
        }
        if (/^endmacro\b/.test(trimmed)) {
          for (let j = i; j >= 0; j--) if (/^macro\b/.test(document.lineAt(j).text.trim())) { ranges.push(new vscode.FoldingRange(j, i, vscode.FoldingRangeKind.Region)); break; }
        }
      }
      if (currentLabel !== null && document.lineCount - currentLabel > 1) ranges.push(new vscode.FoldingRange(currentLabel, document.lineCount - 1, vscode.FoldingRangeKind.Region));
      return ranges;
    }
  }));

  context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider(SCML_SELECTOR, {
    provideDocumentFormattingEdits(document) {
      const edits = [];
      for (let i = 0; i < document.lineCount; i++) {
        const text = document.lineAt(i).text;
        const trimmed = text.trim();
        if (!trimmed) continue;
        const indent = (/^(:|#|endmacro\b)/.test(trimmed)) ? '' : '    ';
        const normalized = indent + trimmed.replace(/\s+,/g, ',').replace(/,\s*/g, ', ');
        if (text !== normalized) edits.push(vscode.TextEdit.replace(document.lineAt(i).range, normalized));
      }
      return edits;
    }
  }));

  context.subscriptions.push(vscode.languages.registerRenameProvider(SCML_SELECTOR, {
    prepareRename(document, position) {
      const w = wordAt(document, position);
      if (!w) throw new Error('No SCML symbol at cursor.');
      return w.range;
    },
    provideRenameEdits(document, position, newName) {
      const w = wordAt(document, position);
      if (!w) return undefined;
      const old = w.text.replace(/^[@:]/, '');
      const edit = new vscode.WorkspaceEdit();
      for (let i = 0; i < document.lineCount; i++) {
        const line = document.lineAt(i).text;
        const re = new RegExp(`([@:])?\\b${old.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`, 'g');
        let m;
        while ((m = re.exec(line))) {
          const prefix = m[1] || '';
          edit.replace(document.uri, new vscode.Range(i, m.index, i, m.index + m[0].length), `${prefix}${newName}`);
        }
      }
      return edit;
    }
  }));

  context.subscriptions.push(vscode.languages.registerCodeActionsProvider(SCML_SELECTOR, {
    provideCodeActions(document, range, context) {
      return context.diagnostics.filter(d => /Unresolved label @/.test(d.message)).map(d => {
        const name = d.message.match(/@([A-Za-z_][A-Za-z0-9_]*)/)[1];
        const action = new vscode.CodeAction(`Create label :${name}`, vscode.CodeActionKind.QuickFix);
        action.edit = new vscode.WorkspaceEdit();
        action.edit.insert(document.uri, new vscode.Position(document.lineCount, 0), `\n:${name}\n0001:\n`);
        action.diagnostics = [d];
        return action;
      });
    }
  }, { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix] }));

  context.subscriptions.push(vscode.workspace.onDidOpenTextDocument(refreshDiagnostics));
  context.subscriptions.push(vscode.workspace.onDidChangeTextDocument(e => refreshDiagnostics(e.document)));
  context.subscriptions.push(vscode.workspace.onDidSaveTextDocument(refreshDiagnostics));
  for (const doc of vscode.workspace.textDocuments) refreshDiagnostics(doc);
}

function deactivate() {
  if (diagnostics) diagnostics.dispose();
}

module.exports = { activate, deactivate, parseDocument, stripComment };
