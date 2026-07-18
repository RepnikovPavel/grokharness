#!/usr/bin/env bash
# Real-model evaluation entrypoint (wrapper around eval_driver.py).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ROOT
export GHAR="${GHAR:-$ROOT/build/ghar}"
export OUT_DIR="${OUT_DIR:-$ROOT/results}"
export SCRATCH="${SCRATCH:-$OUT_DIR/real_model_scratch}"
export REAL_MODEL="${REAL_MODEL:-qwen2.5-coder:1.5b}"
export OLLAMA_HOST="${OLLAMA_HOST:-http://127.0.0.1:11434}"
export TASKS_JSON="${TASKS_JSON:-$ROOT/dont_read_me_src/benchmarks/real_model/tasks.json}"

if [[ ! -x "$GHAR" ]]; then
  echo "Building ghar..."
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j"$(nproc)"
  export GHAR="$ROOT/build/ghar"
fi

exec python3 "$ROOT/dont_read_me_src/benchmarks/real_model/eval_driver.py"
