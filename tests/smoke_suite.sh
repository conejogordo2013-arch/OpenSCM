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
  "examples/rotating_ascii_cube_60fps.scml"
)

for src in "${SAMPLES[@]}"; do
  if [[ -f "$src" ]]; then
    out=".scml/$(basename "${src%.*}").scmlbin"
    echo "[smoke] compile $src -> $out"
    bin/scml compile "$src" "$out"
  fi
done


float_cmp_src=".scml/float_conditions.scml"
float_cmp_bin=".scml/float_conditions.scmlbin"
cat >"$float_cmp_src" <<'SCML'
:MAIN
0004: $A 1.1
0004: $B 1.9
00D9: $A $B @LT_OK
03E5: "float lt failed"
0001:
:LT_OK
00D8: $B $A @GT_OK
03E5: "float gt failed"
0001:
:GT_OK
00D6: $A $B @EQ_BAD
03E5: "float comparisons ok"
0001:
:EQ_BAD
03E5: "float eq failed"
0001:
SCML

echo "[smoke] run float comparison regression"
bin/scml compile "$float_cmp_src" "$float_cmp_bin"
float_cmp_output="$(bin/scml run "$float_cmp_bin")"
if [[ "$float_cmp_output" != "float comparisons ok" ]]; then
  echo "[smoke] unexpected float comparison output: $float_cmp_output" >&2
  exit 1
fi

echo "[smoke] all selected samples compiled and runtime regressions passed"
