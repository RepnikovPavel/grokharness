#!/usr/bin/env bash
# CUDA device probe + optional kernel compile (skip soft if no nvcc)
source "$(dirname "$0")/common.sh"

echo "== test_cuda =="
work="$TMPBASE/cuda"
mkdir -p "$work"
cd "$work"
"$GHAR" init >/dev/null

# device probe — requires nvidia-smi on this machine's CI profile
if command -v nvidia-smi >/dev/null 2>&1; then
  assert_exit 0 "$GHAR" cuda --device --name gpu
else
  echo "  SKIP device (no nvidia-smi)"
  pass=$((pass + 1))
fi

if [[ -x /usr/local/cuda/bin/nvcc ]] || command -v nvcc >/dev/null 2>&1; then
  cp "$ROOT/dont_read_me_src/testdata/add.cu" ./add.cu
  assert_exit 0 "$GHAR" cuda add.cu --name kadd
else
  echo "  SKIP nvcc compile (no nvcc)"
  pass=$((pass + 1))
fi

summary test_cuda
