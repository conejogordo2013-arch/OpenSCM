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


lex_src=".scml/signed_and_assign.scml"
lex_bin=".scml/signed_and_assign.scmlbin"
cat >"$lex_src" <<'SCML'
:MAIN
0004: $X -1.5
0004: $Y 1
$Y += 2
00D6: $Y 3 @PLUS_OK
03E5: "plus assign failed"
0001:
:PLUS_OK
00D9: $X 0.0 @SIGNED_OK
03E5: "signed float failed"
0001:
:SIGNED_OK
03E5: "lexer regressions ok"
0001:
SCML

echo "[smoke] run signed literal and += regression"
bin/scml compile "$lex_src" "$lex_bin"
lex_output="$(bin/scml run "$lex_bin")"
if [[ "$lex_output" != "lexer regressions ok" ]]; then
  echo "[smoke] unexpected lexer regression output: $lex_output" >&2
  exit 1
fi

too_many_src=".scml/too_many_operands.scml"
cat >"$too_many_src" <<'SCML'
:MAIN
CALL_NATIVE "runtime.wait" 1 2 3 4 5 6 7 8 9
0001:
SCML

echo "[smoke] verify compiler rejects extra operands"
if bin/scml compile "$too_many_src" ".scml/too_many_operands.scmlbin" >/tmp/scml_too_many.out 2>&1; then
  echo "[smoke] compiler accepted too many operands" >&2
  cat /tmp/scml_too_many.out >&2
  exit 1
fi

shift_src=".scml/invalid_shift.scml"
shift_bin=".scml/invalid_shift.scmlbin"
cat >"$shift_src" <<'SCML'
:MAIN
0B2B: $X 1 -1
0001:
SCML

echo "[smoke] verify runtime rejects invalid shifts"
bin/scml compile "$shift_src" "$shift_bin"
if bin/scml run "$shift_bin" >/tmp/scml_shift.out 2>&1; then
  echo "[smoke] runtime accepted invalid shift" >&2
  cat /tmp/scml_shift.out >&2
  exit 1
fi

echo "[smoke] all selected samples compiled and runtime regressions passed"
