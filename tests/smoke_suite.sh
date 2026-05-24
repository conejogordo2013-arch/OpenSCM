#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .scml

make bin/scml >/dev/null

SAMPLES=(
  "examples/helloworld.scml"
  "examples/variables.scml"
  "examples/complexlogic.scml"
)

for src in "${SAMPLES[@]}"; do
  if [[ -f "$src" ]]; then
    out=".scml/$(basename "${src%.*}").scmlbin"
    echo "[smoke] compile $src -> $out"
    bin/scml compile "$src" "$out"
  fi
done

echo "[smoke] all selected samples compiled"
