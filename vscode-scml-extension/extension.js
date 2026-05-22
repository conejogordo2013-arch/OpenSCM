const vscode = require('vscode');
const fs = require('fs');
const path = require('path');
const cp = require('child_process');

function resolveScmlExecutable(workspaceFolder) {
  const cfg = vscode.workspace.getConfiguration('scml');
  const configured = cfg.get('executablePath', '').trim();
  if (configured && fs.existsSync(configured)) return configured;
  const candidates = [];
  if (workspaceFolder) {
    candidates.push(path.join(workspaceFolder.uri.fsPath, 'bin', process.platform === 'win32' ? 'scml.exe' : 'scml'));
    candidates.push(path.join(workspaceFolder.uri.fsPath, process.platform === 'win32' ? 'scml.exe' : 'scml'));
  }
  for (const c of candidates) if (fs.existsSync(c)) return c;
  return process.platform === 'win32' ? 'scml.exe' : 'scml';
}

function runCommand(cmd, args, cwd) {
  return new Promise((resolve, reject) => {
    const child = cp.spawn(cmd, args, { cwd, shell: false });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => stdout += d.toString());
    child.stderr.on('data', d => stderr += d.toString());
    child.on('error', reject);
    child.on('close', code => code === 0 ? resolve({ stdout, stderr }) : reject(new Error(stderr || stdout || `Exit code ${code}`)));
  });
}

function getActiveScmlFile() {
  const editor = vscode.window.activeTextEditor;
  if (!editor) throw new Error('No active editor.');
  const file = editor.document.uri.fsPath;
  if (!file.endsWith('.scml') && !file.endsWith('.scmlh')) throw new Error('Active file is not .scml/.scmlh');
  return { file, workspaceFolder: vscode.workspace.getWorkspaceFolder(editor.document.uri) };
}

async function compileCurrent() {
  const { file, workspaceFolder } = getActiveScmlFile();
  const exe = resolveScmlExecutable(workspaceFolder);
  const cfg = vscode.workspace.getConfiguration('scml');
  const outDirCfg = cfg.get('defaultBinOutputDir', '${workspaceFolder}/bin');
  const root = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
  const outDir = outDirCfg.replace('${workspaceFolder}', root);
  fs.mkdirSync(outDir, { recursive: true });
  const outBin = path.join(outDir, path.basename(file).replace(/\.scmlh?$/, '.scmlbin'));
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
  const result = await runCommand(exe, ['compile', file, outBin], cwd);
  if (result.stderr) vscode.window.showWarningMessage(result.stderr.trim());
  vscode.window.showInformationMessage(`SCML compiled: ${outBin}`);
  return outBin;
}

async function runCurrentBinary(binPath) {
  const { workspaceFolder } = getActiveScmlFile();
  const exe = resolveScmlExecutable(workspaceFolder);
  const cfg = vscode.workspace.getConfiguration('scml');
  const args = ['run', binPath];
  if (cfg.get('runWithTrace', false)) args.push('--trace');
  const cwd = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(binPath);
  const result = await runCommand(exe, args, cwd);
  vscode.window.showInformationMessage('SCML run completed. Check Output panel.');
  const out = vscode.window.createOutputChannel('SCML');
  out.appendLine(result.stdout || '');
  if (result.stderr) out.appendLine(result.stderr);
  out.show(true);
}

function activate(context) {
  context.subscriptions.push(vscode.commands.registerCommand('scml.compile', async () => {
    try { await compileCurrent(); } catch (e) { vscode.window.showErrorMessage(`SCML compile failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.run', async () => {
    try {
      const { file, workspaceFolder } = getActiveScmlFile();
      const cfg = vscode.workspace.getConfiguration('scml');
      const root = workspaceFolder ? workspaceFolder.uri.fsPath : path.dirname(file);
      const outDir = cfg.get('defaultBinOutputDir', '${workspaceFolder}/bin').replace('${workspaceFolder}', root);
      const binPath = path.join(outDir, path.basename(file).replace(/\.scmlh?$/, '.scmlbin'));
      await runCurrentBinary(binPath);
    } catch (e) { vscode.window.showErrorMessage(`SCML run failed: ${e.message}`); }
  }));

  context.subscriptions.push(vscode.commands.registerCommand('scml.compileAndRun', async () => {
    try { const bin = await compileCurrent(); await runCurrentBinary(bin); } catch (e) { vscode.window.showErrorMessage(`SCML compile+run failed: ${e.message}`); }
  }));
}

function deactivate() {}
module.exports = { activate, deactivate };
