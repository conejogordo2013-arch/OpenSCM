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
  "examples/rotating_ascii_cubes.scml"
  "examples/hostless_async_static.scml"
  "examples/modern_surface.scml"
  "examples/functions.scml"
  "examples/std_usage.scml"
  "examples/dynamic_arrays.scml"
)

for src in "${SAMPLES[@]}"; do
  if [[ -f "$src" ]]; then
    out=".scml/$(basename "${src%.*}").scmlbin"
    echo "[smoke] compile $src -> $out"
    bin/scml compile "$src" "$out"
  fi
done



modern_bin=".scml/modern_surface.scmlbin"
echo "[smoke] run modern C/C#/SCML surface syntax regression"
bin/scml compile "examples/modern_surface.scml" "$modern_bin"
modern_output="$(bin/scml run "$modern_bin")"
expected_modern_output=$'modern syntax ok
13'
if [[ "$modern_output" != "$expected_modern_output" ]]; then
  echo "[smoke] unexpected modern syntax output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_modern_output" "$modern_output" >&2
  exit 1
fi

modern_bad_src=".scml/modern_unclosed_block.scml"
cat >"$modern_bad_src" <<'SCML'
script MAIN {
    print("missing close")
SCML

echo "[smoke] verify modern syntax rejects unclosed blocks"
if bin/scml compile "$modern_bad_src" ".scml/modern_unclosed_block.scmlbin" >/tmp/scml_modern_bad.out 2>&1; then
  echo "[smoke] compiler accepted an unclosed modern block" >&2
  cat /tmp/scml_modern_bad.out >&2
  exit 1
fi

hostless_bin=".scml/hostless_async_static.scmlbin"
echo "[smoke] run hostless async/static typing regression"
bin/scml compile "examples/hostless_async_static.scml" "$hostless_bin"
hostless_output="$(bin/scml run "$hostless_bin")"
expected_hostless_output=$'SCML hostless async/static demo\nTOTAL\n42\nTYPE_OK\n1'
if [[ "$hostless_output" != "$expected_hostless_output" ]]; then
  echo "[smoke] unexpected hostless async/static output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_hostless_output" "$hostless_output" >&2
  exit 1
fi

static_bad_src=".scml/static_type_mismatch.scml"
cat >"$static_bad_src" <<'SCML'
#include "../std.scmlh"
:MAIN
LET_I32($COUNT, 1)
0004: $COUNT "bad"
0001:
SCML

echo "[smoke] verify compiler rejects static type mismatches"
if bin/scml compile "$static_bad_src" ".scml/static_type_mismatch.scmlbin" >/tmp/scml_static_type.out 2>&1; then
  echo "[smoke] compiler accepted a static type mismatch" >&2
  cat /tmp/scml_static_type.out >&2
  exit 1
fi

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


span_src=".scml/span_render.scml"
span_bin=".scml/span_render.scmlbin"
cat >"$span_src" <<'SCML'
:MAIN
0B43: 4 $BUF
0B44: $BUF
0B45: $BUF 0 4 46
0B46: $BUF 1 64
0B48: $BUF 2 2
0001:
SCML

echo "[smoke] run span framebuffer render regression"
bin/scml compile "$span_src" "$span_bin"
span_output="$(bin/scml run "$span_bin")"
expected_span_output=$'\033[H.@\n..'
if [[ "$span_output" != "$expected_span_output" ]]; then
  echo "[smoke] unexpected span render output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_span_output" "$span_output" >&2
  exit 1
fi

span_bad_src=".scml/span_bad_write.scml"
span_bad_bin=".scml/span_bad_write.scmlbin"
cat >"$span_bad_src" <<'SCML'
:MAIN
0B43: 1 $BUF
0B46: $BUF 1 65
0001:
SCML

echo "[smoke] verify span write bounds checks"
bin/scml compile "$span_bad_src" "$span_bad_bin"
if bin/scml run "$span_bad_bin" >/tmp/scml_span_bad.out 2>&1; then
  echo "[smoke] runtime accepted out-of-range span write" >&2
  cat /tmp/scml_span_bad.out >&2
  exit 1
fi


span_unpinned_src=".scml/span_unpinned_render.scml"
span_unpinned_bin=".scml/span_unpinned_render.scmlbin"
cat >"$span_unpinned_src" <<'SCML'
:MAIN
0B43: 1 $BUF
0B45: $BUF 0 1 46
0B48: $BUF 1 1
0001:
SCML

echo "[smoke] verify framebuffer render requires a pinned span"
bin/scml compile "$span_unpinned_src" "$span_unpinned_bin"
if bin/scml run "$span_unpinned_bin" >/tmp/scml_span_unpinned.out 2>&1; then
  echo "[smoke] runtime rendered an unpinned span" >&2
  cat /tmp/scml_span_unpinned.out >&2
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
    f.write(struct.pack("<4sHIII", b"SCML", 6, 1, 0, 0))
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
    f.write(struct.pack("<4sHIII", b"SCML", 6, 2, 0, 0))
    f.write(bytes([0x3E, 0x00]))
PYBIN
echo "[smoke] verify runtime rejects opcode operand count mismatches"
if bin/scml run "$bad_argc_src" >/tmp/scml_bad_argc.out 2>&1; then
  echo "[smoke] runtime accepted opcode operand count mismatch" >&2
  cat /tmp/scml_bad_argc.out >&2
  exit 1
fi



thread_src=".scml/thread_runtime.scml"
thread_bin=".scml/thread_runtime.scmlbin"
cat >"$thread_src" <<'SCML'
#include "../std.scmlh"
:MAIN
THREAD_CREATE_SLEEP(1, $THREAD)
THREAD_JOIN($THREAD)
THREAD_DONE($THREAD, $DONE)
03E5: "thread runtime ok"
0001:
SCML

echo "[smoke] run host-backed thread runtime regression"
bin/scml compile "$thread_src" "$thread_bin"
thread_output="$(bin/scml run "$thread_bin")"
if [[ "$thread_output" != "thread runtime ok" ]]; then
  echo "[smoke] unexpected thread runtime output: $thread_output" >&2
  exit 1
fi

project_dir=".scml/project_tooling"
rm -rf "$project_dir"
mkdir -p "$project_dir/src" "$project_dir/packages/mathkit"
cat >"$project_dir/scml.pkg" <<'SCML'
name = "project-tooling"
source = "src/main.scml"
package = "packages/mathkit"
output = "build/project-tooling.scmlbin"
jobs = 4
SCML
cat >"$project_dir/packages/mathkit/mathkit.scmlh" <<'SCML'
#pragma once
macro PLUS_TEN(outvar, value):
    0006: outvar value 10
endmacro
SCML
cat >"$project_dir/src/main.scml" <<'SCML'
#include "../../std.scmlh"
#include <mathkit.scmlh>
:MAIN
LET_U32($COUNT, 32)
LET_ARRAY($ITEMS, 0)
PLUS_TEN($TOTAL, $COUNT)
TYPE_ASSERT($TOTAL, "u32", $TYPE_OK)
03E5: "project tooling ok"
03E5: $TOTAL
03E5: $TYPE_OK
0001:
SCML

echo "[smoke] build manifest project with package include and rich type aliases"
bin/scml build "$project_dir/scml.pkg"
bin/scml check "$project_dir/scml.pkg"
project_output="$(bin/scml run "$project_dir/build/project-tooling.scmlbin")"
expected_project_output=$'project tooling ok
42
1'
if [[ "$project_output" != "$expected_project_output" ]]; then
  echo "[smoke] unexpected project tooling output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_project_output" "$project_output" >&2
  exit 1
fi

echo "[smoke] run migration audit"
bash tools/scml_migration_audit.sh

echo "[smoke] all selected samples compiled and runtime regressions passed"
