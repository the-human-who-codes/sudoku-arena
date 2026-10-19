#!/usr/bin/env bash
# Compiles the C++ engine to WebAssembly and copies it into the web app.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

if ! command -v emcmake >/dev/null 2>&1; then
  for env in "${EMSDK:-}/emsdk_env.sh" "$HOME/.emsdk/emsdk_env.sh" "$HOME/emsdk/emsdk_env.sh"; do
    if [ -f "$env" ]; then
      # shellcheck disable=SC1090
      source "$env" >/dev/null 2>&1
      break
    fi
  done
fi
if ! command -v emcmake >/dev/null 2>&1; then
  echo "Emscripten not found. Install emsdk: https://emscripten.org/docs/getting_started/downloads.html" >&2
  exit 1
fi

emcmake cmake -S "$root/engine" -B "$root/build/wasm" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$root/build/wasm" --parallel

out="$root/web/src/engine/generated"
mkdir -p "$out"
cp "$root/build/wasm/sudoku.mjs" "$root/build/wasm/sudoku.wasm" "$root/build/wasm/sudoku.d.mts" "$out/"
echo "Engine written to ${out#"$root"/}"
