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
  "examples/advanced_control_flow.scml"
  "examples/advanced_language_surface.scml"
  "examples/functions.scml"
  "examples/std_usage.scml"
  "examples/dynamic_arrays.scml"
  "examples/universal_runtime_data.scml"
  "examples/std_modules/io_collections.scml"
  "examples/std_modules/types_ranges.scml"
  "examples/std_modules/data_ffi_vm.scml"
  "examples/std_modules/fs_concurrency_meta.scml"
  "examples/sdl2_ffi/linux_sdl2_window_ffi.scml"
  "examples/sdl2_ffi/msys2_ucrt64_sdl2_window_ffi.scml"
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


advanced_control_bin=".scml/advanced_control_flow.scmlbin"
echo "[smoke] run advanced modern control-flow regression"
bin/scml compile "examples/advanced_control_flow.scml" "$advanced_control_bin"
advanced_control_output="$(bin/scml run "$advanced_control_bin")"
if [[ "$advanced_control_output" != "8" ]]; then
  echo "[smoke] unexpected advanced control-flow output: $advanced_control_output" >&2
  exit 1
fi


advanced_surface_bin=".scml/advanced_language_surface.scmlbin"
echo "[smoke] run advanced language surface regression"
bin/scml compile "examples/advanced_language_surface.scml" "$advanced_surface_bin"
advanced_surface_output="$(bin/scml run "$advanced_surface_bin")"
expected_advanced_surface_output=$'42
0
0
42
99
3
advanced surface ok'
if [[ "$advanced_surface_output" != "$expected_advanced_surface_output" ]]; then
  echo "[smoke] unexpected advanced language surface output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_advanced_surface_output" "$advanced_surface_output" >&2
  exit 1
fi

cpp17_bin=".scml/cpp17_superiority.scmlbin"
echo "[smoke] run SCML C++17 superset regression"
bin/scml compile "examples/cpp17_superiority.scml" "$cpp17_bin"
cpp17_output="$(bin/scml run "$cpp17_bin")"
expected_cpp17_output=$'if-init ok
120
24
constexpr ok
1
120
17
24
1'
if [[ "$cpp17_output" != "$expected_cpp17_output" ]]; then
  echo "[smoke] unexpected C++17 superset output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_cpp17_output" "$cpp17_output" >&2
  exit 1
fi

cpp20_bin=".scml/cpp20_domination.scmlbin"
echo "[smoke] run SCML C++20 domination regression"
bin/scml compile "examples/cpp20_domination.scml" "$cpp20_bin"
cpp20_output="$(bin/scml run "$cpp20_bin")"
expected_cpp20_output=$'5
10
33
45
1
42'
if [[ "$cpp20_output" != "$expected_cpp20_output" ]]; then
  echo "[smoke] unexpected C++20 domination output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_cpp20_output" "$cpp20_output" >&2
  exit 1
fi

else_if_src=".scml/else_if_control.scml"
else_if_bin=".scml/else_if_control.scmlbin"
cat >"$else_if_src" <<'SCML'
script MAIN {
    let $value: i32 = 1;
    let $seen: i32 = 0;

    if ($value == 1) {
        $seen = 10;
    } else if ($value == 1) {
        $seen = 20;
    } else {
        $seen = 30;
    }
    print($seen);

    if ($value == 2) {
        $seen = 40;
    } else if ($value == 3) {
        $seen = 50;
    } else {
        $seen = 60;
    }
    print($seen);
}
SCML

echo "[smoke] run modern else-if chain regression"
bin/scml compile "$else_if_src" "$else_if_bin"
else_if_output="$(bin/scml run "$else_if_bin")"
expected_else_if_output=$'10
60'
if [[ "$else_if_output" != "$expected_else_if_output" ]]; then
  echo "[smoke] unexpected else-if output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_else_if_output" "$else_if_output" >&2
  exit 1
fi

break_bad_src=".scml/break_outside_loop.scml"
cat >"$break_bad_src" <<'SCML'
script MAIN {
    break;
}
SCML

echo "[smoke] verify modern syntax rejects break outside loops"
if bin/scml compile "$break_bad_src" ".scml/break_outside_loop.scmlbin" >/tmp/scml_break_bad.out 2>&1; then
  echo "[smoke] compiler accepted break outside loop" >&2
  cat /tmp/scml_break_bad.out >&2
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
CALL_NATIVE "runtime.wait" 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16
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

if pkg-config --exists libffi 2>/dev/null; then
  ffi_native_lib="examples/libscml_ffi_native.so"
  ffi_native_src=".scml/ffi_native_many_args.scml"
  ffi_native_bin=".scml/ffi_native_many_args.scmlbin"
  ${CC:-cc} -shared -fPIC -o "$ffi_native_lib" examples/ffi_native.c
  cat >"$ffi_native_src" <<'SCML'
:MAIN
0B31: "ffi.add_search_path" "examples"
0B31: "ffi.load" "libscml_ffi_native"
0B31: "ffi.declare" "scml_ffi_sum15_i32" "int" "int,int,int,int,int,int,int,int,int,int,int,int,int,int,int"
0B31: "scml_ffi_sum15_i32" 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
03E5: $RETVAL
0001:
SCML

  echo "[smoke] run libffi many-argument native call regression"
  bin/scml compile "$ffi_native_src" "$ffi_native_bin"
  ffi_native_output="$(bin/scml run "$ffi_native_bin")"
  if [[ "$ffi_native_output" != "120" ]]; then
    echo "[smoke] unexpected many-argument FFI output: $ffi_native_output" >&2
    rm -f "$ffi_native_lib"
    exit 1
  fi

  ffi_pointer_src=".scml/ffi_pointer_vtable.scml"
  ffi_pointer_bin=".scml/ffi_pointer_vtable.scmlbin"
  cat >"$ffi_pointer_src" <<'SCML'
:MAIN
0B31: "ffi.add_search_path" "examples"
0B31: "ffi.load" "libscml_ffi_native"
0B31: "ffi.abi_supported" "cdecl"
03E5: $RETVAL
0B31: "ffi.declare" "scml_ffi_add_i32_ptr" "pointer" ""
0B31: "scml_ffi_add_i32_ptr"
0004: $ADD_PTR $RETVAL
0B31: "ffi.call_ptr" $ADD_PTR "int" "int,int" 5 6
03E5: $RETVAL
0B31: "ffi.declare" "scml_ffi_fake_object" "pointer" ""
0B31: "scml_ffi_fake_object"
0004: $OBJ $RETVAL
0B31: "ffi.vtable_call" $OBJ 0 "int" "int" 32
03E5: $RETVAL
0001:
SCML
  echo "[smoke] run FFI function pointer/vtable regression"
  bin/scml compile "$ffi_pointer_src" "$ffi_pointer_bin"
  ffi_pointer_output="$(bin/scml run "$ffi_pointer_bin")"
  rm -f "$ffi_native_lib"
  expected_ffi_pointer_output=$'1
11
42'
  if [[ "$ffi_pointer_output" != "$expected_ffi_pointer_output" ]]; then
    echo "[smoke] unexpected FFI function pointer/vtable output" >&2
    printf 'expected: %q\nactual:   %q\n' "$expected_ffi_pointer_output" "$ffi_pointer_output" >&2
    exit 1
  fi
else
  echo "[smoke] skip libffi many-argument native call regression (libffi unavailable)"
fi

ffi_advanced_src=".scml/ffi_advanced.scml"
ffi_advanced_bin=".scml/ffi_advanced.scmlbin"
cat >"$ffi_advanced_src" <<'SCML'
:MAIN
0B31: "ffi.struct_define" "Vec3" "float:x,float:y,float:z,uint16:flags"
0B31: "ffi.struct_size" "Vec3"
0004: $SZ $RETVAL
0B31: "ffi.alloc" $SZ
0004: $VEC $RETVAL
0B31: "ffi.struct_write" $VEC "Vec3" "x" 1.5
0B31: "ffi.struct_write" $VEC "Vec3" "flags" 513
0B31: "ffi.struct_read" $VEC "Vec3" "x"
03E5: $RETVAL
0B31: "ffi.struct_read" $VEC "Vec3" "flags"
03E5: $RETVAL
0B31: "ffi.alloc_array" 4 "uint16"
0004: $ARR $RETVAL
0B31: "ffi.array_write" $ARR 2 "uint16" 655
0B31: "ffi.array_read" $ARR 2 "uint16"
03E5: $RETVAL
0B31: "ffi.free" $ARR
0B31: "ffi.free" $VEC
0001:
SCML

echo "[smoke] run advanced FFI struct/array regression"
bin/scml compile "$ffi_advanced_src" "$ffi_advanced_bin"
ffi_advanced_output="$(bin/scml run "$ffi_advanced_bin")"
expected_ffi_advanced_output=$'1.5\n513\n655'
if [[ "$ffi_advanced_output" != "$expected_ffi_advanced_output" ]]; then
  echo "[smoke] unexpected advanced FFI output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_ffi_advanced_output" "$ffi_advanced_output" >&2
  exit 1
fi

ffi_more_advanced_src=".scml/ffi_more_advanced.scml"
ffi_more_advanced_bin=".scml/ffi_more_advanced.scmlbin"
cat >"$ffi_more_advanced_src" <<'SCML'
:MAIN
0B31: "ffi.struct_define" "Packet" "uint32:id,uint16:flags,uint8:kind"
0B31: "ffi.alloc_struct_array" "Packet" 3
0004: $PACKETS $RETVAL
0B31: "ffi.struct_array_write" $PACKETS 1 "Packet" "id" 12345
0B31: "ffi.struct_array_write" $PACKETS 1 "Packet" "kind" 7
0B31: "ffi.struct_array_read" $PACKETS 1 "Packet" "kind"
03E5: $RETVAL
0B31: "ffi.struct_ptr" $PACKETS "Packet" 1
0004: $P1 $RETVAL
0B31: "ffi.struct_read" $P1 "Packet" "id"
03E5: $RETVAL
0B31: "ffi.struct_field_ptr" $P1 "Packet" "flags"
0004: $FLAGS $RETVAL
0B31: "ffi.write" $FLAGS 0 "uint16" 48879
0B31: "ffi.struct_array_read" $PACKETS 1 "Packet" "flags"
03E5: $RETVAL
0B31: "ffi.ptr_diff" $P1 $PACKETS
03E5: $RETVAL
0B31: "ffi.alloc" 8
0004: $TEXT $RETVAL
0B31: "ffi.write_cstring" $TEXT 0 "abcdefghi" 8
0B31: "ffi.read_cstring" $TEXT
03E5: $RETVAL
0B31: "ffi.realloc" $TEXT 16
0004: $TEXT $RETVAL
0B31: "ffi.write_cstring" $TEXT 7 "Z" 2
0B31: "ffi.read_cstring" $TEXT
03E5: $RETVAL
0B31: "ffi.free" $TEXT
0B31: "ffi.free" $PACKETS
0001:
SCML

echo "[smoke] run advanced FFI struct arrays/pointers/cstrings regression"
bin/scml compile "$ffi_more_advanced_src" "$ffi_more_advanced_bin"
ffi_more_advanced_output="$(bin/scml run "$ffi_more_advanced_bin")"
expected_ffi_more_advanced_output=$'7\n0x3039\n48879\n0x8\nabcdefg\nabcdefgZ'
if [[ "$ffi_more_advanced_output" != "$expected_ffi_more_advanced_output" ]]; then
  echo "[smoke] unexpected advanced FFI pointer/cstring output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_ffi_more_advanced_output" "$ffi_more_advanced_output" >&2
  exit 1
fi

ffi_native_layout_src=".scml/ffi_native_layout.scml"
ffi_native_layout_bin=".scml/ffi_native_layout.scmlbin"
cat >"$ffi_native_layout_src" <<'SCML'
:MAIN
0B31: "ffi.last_error"
03E5: $RETVAL
0B31: "ffi.utf16" "hello utf16"
0004: $WIDE $RETVAL
0B31: "ffi.read_utf16" $WIDE
03E5: $RETVAL
0B31: "ffi.free" $WIDE
0B31: "ffi.union_begin" "NumberBits"
0B31: "ffi.struct_field" "NumberBits" "as_i32" "int"
0B31: "ffi.struct_field" "NumberBits" "as_u64" "uint64"
0B31: "ffi.struct_finish" "NumberBits"
0B31: "ffi.struct_offset" "NumberBits" "as_u64"
03E5: $RETVAL
0B31: "ffi.struct_size" "NumberBits"
03E5: $RETVAL
0001:
SCML

echo "[smoke] run FFI UTF-16/union layout regression"
bin/scml compile "$ffi_native_layout_src" "$ffi_native_layout_bin"
ffi_native_layout_output="$(bin/scml run "$ffi_native_layout_bin")"
expected_ffi_native_layout_output=$'no FFI error
hello utf16
0
8'
if [[ "$ffi_native_layout_output" != "$expected_ffi_native_layout_output" ]]; then
  echo "[smoke] unexpected FFI UTF-16/union layout output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_ffi_native_layout_output" "$ffi_native_layout_output" >&2
  exit 1
fi

ffi_struct_array_fields_src=".scml/ffi_struct_array_fields.scml"
ffi_struct_array_fields_bin=".scml/ffi_struct_array_fields.scmlbin"
cat >"$ffi_struct_array_fields_src" <<'SCML'
:MAIN
0B31: "ffi.struct_define" "Header" "uint32:id,uint8[4]:tag,uint16[3]:scores"
0B31: "ffi.alloc_struct" "Header"
0004: $H $RETVAL
0B31: "ffi.struct_field_write" $H "Header" "scores" 2 900
0B31: "ffi.struct_field_read" $H "Header" "scores" 2
03E5: $RETVAL
0B31: "ffi.struct_field_ptr" $H "Header" "tag" 0
0004: $TAG $RETVAL
0B31: "ffi.write_cstring" $TAG 0 "XYZW" 4
0B31: "ffi.read_cstring" $TAG
03E5: $RETVAL
0B31: "ffi.struct_field_ptr" $H "Header" "scores" 2
0004: $SCORE2 $RETVAL
0B31: "ffi.ptr_diff" $SCORE2 $H
03E5: $RETVAL
0B31: "ffi.free" $H
0001:
SCML

echo "[smoke] run FFI fixed-array struct field regression"
bin/scml compile "$ffi_struct_array_fields_src" "$ffi_struct_array_fields_bin"
ffi_struct_array_fields_output="$(bin/scml run "$ffi_struct_array_fields_bin")"
expected_ffi_struct_array_fields_output=$'900
XYZ
0xc'
if [[ "$ffi_struct_array_fields_output" != "$expected_ffi_struct_array_fields_output" ]]; then
  echo "[smoke] unexpected FFI fixed-array struct output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_ffi_struct_array_fields_output" "$ffi_struct_array_fields_output" >&2
  exit 1
fi

ffi_memory_bytes_src=".scml/ffi_memory_bytes.scml"
ffi_memory_bytes_bin=".scml/ffi_memory_bytes.scmlbin"
cat >"$ffi_memory_bytes_src" <<'SCML'
use "../std.scmlh"
:MAIN
0B31: "ffi.alloc" 8
0004: $BUF $RETVAL
SCML_FFI_WRITE_BYTES($BUF, 0, "DE AD BE EF", $OK)
03E5: $OK
SCML_FFI_READ_BYTES($BUF, 0, 4, $HEX)
03E5: $HEX
0B31: "ffi.ptr_add" $BUF 2
0004: $P2 $RETVAL
0B31: "ffi.ptr_diff" $P2 $BUF
03E5: $RETVAL
SCML_FFI_NULL($NULL)
SCML_FFI_IS_NULL($NULL, $IS_NULL)
03E5: $IS_NULL
0B31: "ffi.alloc" 8
0004: $OTHER $RETVAL
SCML_FFI_MEMMOVE($OTHER, $BUF, 4, $MOVE_OK)
03E5: $MOVE_OK
SCML_FFI_MEMCMP($BUF, $OTHER, 4, $CMP)
03E5: $CMP
0B31: "ffi.free" $OTHER
0B31: "ffi.free" $BUF
0001:
SCML

echo "[smoke] run FFI byte/pointer helper regression"
bin/scml compile "$ffi_memory_bytes_src" "$ffi_memory_bytes_bin"
ffi_memory_bytes_output="$(bin/scml run "$ffi_memory_bytes_bin")"
expected_ffi_memory_bytes_output=$'1
DEADBEEF
0x2
1
1
0'
if [[ "$ffi_memory_bytes_output" != "$expected_ffi_memory_bytes_output" ]]; then
  echo "[smoke] unexpected FFI byte/pointer helper output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_ffi_memory_bytes_output" "$ffi_memory_bytes_output" >&2
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

mega_dir=".scml/mega_project_tooling"
rm -rf "$mega_dir"
mkdir -p "$mega_dir/src/core" "$mega_dir/packages"
cat >"$mega_dir/scml.pkg" <<'SCML'
name = "mega-project-tooling"
source_dir = "src"
package_dir = "packages"
output = "build/nested/mega.scmlbin"
jobs = 8
define = "PROJECT_MODE mega # hash-inside-quotes"
SCML
cat >"$mega_dir/src/main.scml" <<'SCML'
:MAIN
#ifdef PROJECT_MODE
03E5: "define ok"
#endif
#for N in "ONE","TWO","THREE":
03E5: N
#endfor
0001:
SCML

echo "[smoke] build source_dir project with manifest defines and #for metaprogramming"
bin/scml build "$mega_dir/scml.pkg"
bin/scml metadata "$mega_dir/scml.pkg" >/tmp/scml_mega_meta.out
if ! rg -q "sources \(1\)" /tmp/scml_mega_meta.out || ! rg -q "PROJECT_MODE mega" /tmp/scml_mega_meta.out; then
  echo "[smoke] metadata did not report expected source/define" >&2
  cat /tmp/scml_mega_meta.out >&2
  exit 1
fi
mega_output="$(bin/scml run "$mega_dir/build/nested/mega.scmlbin")"
expected_mega_output=$'define ok
ONE
TWO
THREE'
if [[ "$mega_output" != "$expected_mega_output" ]]; then
  echo "[smoke] unexpected mega tooling output" >&2
  printf 'expected: %q
actual:   %q
' "$expected_mega_output" "$mega_output" >&2
  exit 1
fi

echo "[smoke] run migration audit"
bash tools/scml_migration_audit.sh


numeric_src=".scml/numeric_literals_regression.scml"
numeric_bin=".scml/numeric_literals_regression.scmlbin"
cat >"$numeric_src" <<'SCML'
:MAIN
0004: $A +1.5
0004: $B 010
00D6: $B 10 @DECIMAL_OK
03E5: "leading zero decimal failed"
0001:
:DECIMAL_OK
00D8: $A 1.0 @PLUS_FLOAT_OK
03E5: "plus float failed"
0001:
:PLUS_FLOAT_OK
03E5: "numeric literals ok"
0001:
SCML

echo "[smoke] run numeric literal parsing regression"
bin/scml compile "$numeric_src" "$numeric_bin"
numeric_output="$(bin/scml run "$numeric_bin")"
if [[ "$numeric_output" != "numeric literals ok" ]]; then
  echo "[smoke] unexpected numeric literal output: $numeric_output" >&2
  exit 1
fi

entity_src=".scml/entity_set_runtime.scml"
entity_bin=".scml/entity_set_runtime.scmlbin"
cat >"$entity_src" <<'SCML'
:MAIN
0B00: "worker" 1 2 3 $E
0B01: $E "health" 100
03E5: "entity set ok"
0001:
SCML

echo "[smoke] run ENTITY_SET runtime dispatch regression"
bin/scml compile "$entity_src" "$entity_bin"
entity_output="$(bin/scml run "$entity_bin")"
if [[ "$entity_output" != "entity set ok" ]]; then
  echo "[smoke] unexpected entity set output: $entity_output" >&2
  exit 1
fi


backend_src=".scml/backend_registry_runtime.scml"
backend_bin=".scml/backend_registry_runtime.scmlbin"
cat >"$backend_src" <<'SCML'
:MAIN
0B31: "runtime.backend_info" "gpu"
0004: $INFO $RETVAL
03E5: $INFO
0B31: "runtime.select_backend" "gpu" "missing_backend"
0004: $BAD $RETVAL
03E5: $BAD
0B31: "runtime.select_backend" "gpu" "default"
0004: $OK $RETVAL
03E5: $OK
0001:
SCML

echo "[smoke] run runtime backend registry regression"
bin/scml compile "$backend_src" "$backend_bin"
backend_output="$(bin/scml run "$backend_bin")"
if [[ "$backend_output" != module=gpu\ active=*backends=*compiled=*$'\n0\n1' ]]; then
  echo "[smoke] unexpected backend registry output" >&2
  printf 'actual: %q\n' "$backend_output" >&2
  exit 1
fi

universal_data_bin=".scml/universal_runtime_data.scmlbin"
echo "[smoke] run universal data/env runtime regression"
bin/scml compile "examples/universal_runtime_data.scml" "$universal_data_bin"
universal_data_output="$(bin/scml run "$universal_data_bin")"
expected_universal_data_output=$'SCML%20hace%20de%20todo\nSCML hace de todo\nSCML\n9000\ndata\n1\nok'
if [[ "$universal_data_output" != "$expected_universal_data_output" ]]; then
  echo "[smoke] unexpected universal data/env output" >&2
  printf 'expected: %q\nactual:   %q\n' "$expected_universal_data_output" "$universal_data_output" >&2
  exit 1
fi

bad_store_bin=".scml/bad_store_destination.scmlbin"
python3 - <<'PY' "$bad_store_bin"
import struct, sys
path = sys.argv[1]
magic = 0x4C4D4353
version = 6
code = bytes([0x04, 0x02, 0x01]) + struct.pack('<i', 0) + bytes([0x01]) + struct.pack('<i', 1)
with open(path, 'wb') as f:
    f.write(struct.pack('<I H I I I', magic, version, len(code), 0, 0))
    f.write(code)
PY

echo "[smoke] verify runtime rejects invalid STORE destination operand"
if bin/scml run "$bad_store_bin" >/tmp/scml_bad_store.out 2>&1; then
  echo "[smoke] runtime accepted invalid STORE destination" >&2
  cat /tmp/scml_bad_store.out >&2
  exit 1
fi

bad_jump_bin=".scml/bad_jump_target.scmlbin"
python3 - <<'PY' "$bad_jump_bin"
import struct, sys
path = sys.argv[1]
magic = 0x4C4D4353
version = 6
code = bytes([0x0A, 0x01, 0x04]) + struct.pack('<I', 999999)
with open(path, 'wb') as f:
    f.write(struct.pack('<I H I I I', magic, version, len(code), 0, 0))
    f.write(code)
PY

echo "[smoke] verify runtime rejects invalid jump targets"
if bin/scml run "$bad_jump_bin" >/tmp/scml_bad_jump.out 2>&1; then
  echo "[smoke] runtime accepted invalid jump target" >&2
  cat /tmp/scml_bad_jump.out >&2
  exit 1
fi

echo "[smoke] all selected samples compiled and runtime regressions passed"
