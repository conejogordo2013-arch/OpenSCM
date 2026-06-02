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

std_hits=0
while IFS=: read -r file line_no macro_name; do
  case "$file" in
    stscm/compat/legacy_scml_prefix.scmlh|scmlspec/*|examples/*|tests/*) ;;
    *)
      printf '[std-freeze] SCML_* macro outside compat/examples/spec/tests in %s:%s:%s\n' "$file" "$line_no" "$macro_name" >&2
      std_hits=$((std_hits + 1))
      ;;
  esac
done < <(python3 - <<'PY'
from pathlib import Path
import re
for path in Path('.').rglob('*'):
    if path.is_file() and path.suffix in {'.scml', '.scmlh'}:
        text = path.read_text(errors='ignore').splitlines()
        for i, line in enumerate(text, 1):
            m = re.match(r'\s*macro\s+(SCML_[A-Z0-9_]+)\s*\(', line)
            if m:
                print(f'{path.as_posix()}:{i}:{m.group(1)}')
PY
)

module_hits=0
for module in stscm/modules/*.scmlh; do
  [[ -e "$module" ]] || continue
  while IFS=: read -r line_no macro_name; do
    if [[ ! "$macro_name" =~ ^[a-z][a-z0-9_]*$ ]]; then
      printf '[std-modules] non lower_snake_case macro in %s:%s:%s\n' "$module" "$line_no" "$macro_name" >&2
      module_hits=$((module_hits + 1))
    fi
  done < <(python3 - "$module" <<'PY'
import re, sys
for i, line in enumerate(open(sys.argv[1], encoding='utf-8'), 1):
    m = re.match(r'\s*macro\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', line)
    if m:
        print(f'{i}:{m.group(1)}')
PY
  )
done

if (( legacy_hits > 0 || std_hits > 0 || module_hits > 0 )); then
  echo "[migration] failed: legacy_hits=$legacy_hits std_hits=$std_hits module_hits=$module_hits" >&2
  exit 1
fi

printf '[migration] OK: source tree uses modern use/import and brace macro declarations\n'
printf '[std-freeze] OK: SCML_* compatibility macros are isolated in stscm/compat/legacy_scml_prefix.scmlh\n'
printf '[std-modules] OK: official modules use lower_snake_case names\n'
