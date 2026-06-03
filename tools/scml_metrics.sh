#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

count_lines() {
  local pattern=$1
  shift
  python3 - "$pattern" "$@" <<'PY'
from pathlib import Path
import sys
suffixes=sys.argv[1].split(',')
roots=sys.argv[2:] or ['.']
total=0
for root in roots:
    for p in Path(root).rglob('*'):
        if not p.is_file() or p.suffix not in suffixes:
            continue
        if any(part in {'.git','bin','.scml','build'} for part in p.parts):
            continue
        for line in p.read_text(errors='ignore').splitlines():
            stripped=line.strip()
            if stripped and not stripped.startswith(';') and not stripped.startswith('//'):
                total += 1
print(total)
PY
}

pure_scml_functions=$(python3 - <<'PY'
from pathlib import Path
import re
count=0
for p in Path('.').rglob('*.scml'):
    if any(part in {'.git','bin','.scml','build'} for part in p.parts):
        continue
    for line in p.read_text(errors='ignore').splitlines():
        if re.match(r'\s*(fn|function)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', line):
            count += 1
print(count)
PY
)
std_functions=$(python3 - <<'PY'
from pathlib import Path
import re
count=0
for p in Path('stscm').rglob('*.scmlh'):
    for line in p.read_text(errors='ignore').splitlines():
        if re.match(r'\s*macro\s+[A-Za-z_][A-Za-z0-9_]*\s*\(', line):
            count += 1
print(count)
PY
)
c_functions=$(python3 - <<'PY'
from pathlib import Path
import re
count=0
pat=re.compile(r'^[A-Za-z_][A-Za-z0-9_\s\*]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\([^;]*\)\s*\{')
for root in ['.']:
    for p in Path(root).rglob('*'):
        if p.suffix not in {'.c','.h'} or any(part in {'.git','bin','.scml','build'} for part in p.parts):
            continue
        for line in p.read_text(errors='ignore').splitlines():
            st=line.strip()
            if pat.match(st) and not st.startswith(('if ', 'for ', 'while ', 'switch ')):
                count += 1
print(count)
PY
)
c_loc=$(count_lines .c,.h .)
scml_std_loc=$(count_lines .scml,.scmlh stscm)
scml_all_loc=$(count_lines .scml,.scmlh .)

total_functions=$((pure_scml_functions + std_functions))
total_loc=$((c_loc + scml_all_loc))
cat <<REPORT
SCML pure functions: $pure_scml_functions
SCML STD macro functions: $std_functions
SCML pure + STD functions: $total_functions
C LOC: $c_loc
SCML STD LOC: $scml_std_loc
All SCML LOC: $scml_all_loc
C + all SCML LOC: $total_loc
C function definitions: $c_functions
REPORT
