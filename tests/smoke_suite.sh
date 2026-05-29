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

terminal_src=".scml/terminal_controls.scml"
terminal_bin=".scml/terminal_controls.scmlbin"
cat >"$terminal_src" <<'SCML'
:MAIN
0B3E: 196 16
0B42: 1
0B40: 2 3
0B41:
0B3F:
0B32: "X"
0001:
SCML

echo "[smoke] run terminal control regression"
bin/scml compile "$terminal_src" "$terminal_bin"
terminal_output="$(bin/scml run "$terminal_bin")"
expected_terminal_output=$'\033[38;5;196m\033[48;5;16m\033[1m\033[2;3H\033[2K\033[0mX'
if [[ "$terminal_output" != "$expected_terminal_output" ]]; then
  echo "[smoke] unexpected terminal control output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_terminal_output" "$terminal_output" >&2
  exit 1
fi

bad_color_src=".scml/invalid_terminal_color.scml"
bad_color_bin=".scml/invalid_terminal_color.scmlbin"
cat >"$bad_color_src" <<'SCML'
:MAIN
0B3E: 300
0001:
SCML

echo "[smoke] verify runtime rejects invalid terminal color"
bin/scml compile "$bad_color_src" "$bad_color_bin"
if bin/scml run "$bad_color_bin" >/tmp/scml_bad_color.out 2>&1; then
  echo "[smoke] runtime accepted invalid terminal color" >&2
  cat /tmp/scml_bad_color.out >&2
  exit 1
fi

truncated_src=".scml/truncated_header.scmlbin"
python3 - <<'PYBIN'
import struct
with open(".scml/truncated_header.scmlbin", "wb") as f:
    f.write(struct.pack("<4sHIII", b"SCML", 5, 1, 0, 0))
    f.write(b"\x00")
PYBIN
echo "[smoke] verify runtime rejects truncated instruction headers"
if bin/scml run "$truncated_src" >/tmp/scml_truncated.out 2>&1; then
  echo "[smoke] runtime accepted truncated instruction header" >&2
  cat /tmp/scml_truncated.out >&2
  exit 1
fi

bad_argc_src=".scml/bad_opcode_argc.scmlbin"
python3 - <<'PYBIN'
import struct
with open(".scml/bad_opcode_argc.scmlbin", "wb") as f:
    f.write(struct.pack("<4sHIII", b"SCML", 5, 2, 0, 0))
    f.write(bytes([0x3E, 0x00]))
PYBIN
echo "[smoke] verify runtime rejects opcode operand count mismatches"
if bin/scml run "$bad_argc_src" >/tmp/scml_bad_argc.out 2>&1; then
  echo "[smoke] runtime accepted opcode operand count mismatch" >&2
  cat /tmp/scml_bad_argc.out >&2
  exit 1
fi

echo "[smoke] all selected samples compiled and runtime regressions passed"
