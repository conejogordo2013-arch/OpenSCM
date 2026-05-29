#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

legacy_hits=0
while IFS= read -r file; do
  while IFS=: read -r line_no line_text; do
    printf '[migration] legacy syntax remains in %s:%s:%s\n' "$file" "$line_no" "$line_text" >&2
    legacy_hits=$((legacy_hits + 1))
  done < <(awk '
    /^[[:space:]]*(;|\/\/)/ { next }
    /^[[:space:]]*#include[[:space:]]/ { print FNR ":" $0 }
    /^[[:space:]]*endmacro[[:space:]]*$/ { print FNR ":" $0 }
    /^[[:space:]]*macro[[:space:]][^\{]*:[[:space:]]*$/ { print FNR ":" $0 }
  ' "$file")
done < <(rg --files -g '*.scml' -g '*.scmlh' -g '!bin/**' -g '!build/**' -g '!.scml/**')

if (( legacy_hits > 0 )); then
  echo "[migration] failed: found $legacy_hits legacy include/macro declarations" >&2
  exit 1
fi

echo "[migration] OK: source tree uses modern use/import and brace macro declarations"
