#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

mkdir -p .scml/verify
count=0

while IFS= read -r src; do
  out=".scml/verify/${src//[\/ ]/_}.scmlbin"
  mkdir -p "$(dirname "$out")"
  if [[ "$src" == "examples/enterprise_project/src/main.scml" ]]; then
    continue
  elif [[ "$src" == "examples/modular_main.scml" ]]; then
    ./bin/scml compile "$src" examples/modular_functions.scml "$out" >/dev/null
  else
    ./bin/scml compile "$src" "$out" >/dev/null
  fi
  count=$((count + 1))
done < <(find examples -name '*.scml' -print | sort)

./bin/scml build examples/enterprise_project/scml.pkg >/dev/null

printf '[examples] OK: compiled %d standalone examples and enterprise project manifest\n' "$count"
