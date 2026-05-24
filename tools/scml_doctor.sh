#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[doctor] SCML system diagnostics"
command -v "${CC:-cc}" >/dev/null 2>&1 || { echo "[doctor] missing C compiler"; exit 1; }
command -v make >/dev/null 2>&1 || { echo "[doctor] missing make"; exit 1; }

echo "[doctor] building core CLI"
make bin/scml

echo "[doctor] running compiler smoke"
bin/scml compile examples/helloworld.scml .scml/helloworld.scmlbin
bin/scml compile examples/variables.scml .scml/variables.scmlbin
bin/scml compile examples/complexlogic.scml .scml/complexlogic.scmlbin

echo "[doctor] OK"
