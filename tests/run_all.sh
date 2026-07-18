#!/usr/bin/env bash
# Run all integration tests. Exit 0 only if every suite passes.
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
export GHAR="${GHAR:-$ROOT/build/ghar}"

if [[ ! -x "$GHAR" ]]; then
  echo "Building ghar..."
  cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
  cmake --build "$ROOT/build" -j"$(nproc)"
  GHAR="$ROOT/build/ghar"
fi

echo "Using GHAR=$GHAR"
failed=0
for t in "$DIR"/test_*.sh; do
  echo
  if bash "$t"; then
    echo "PASS $(basename "$t")"
  else
    echo "FAIL $(basename "$t")" >&2
    failed=$((failed + 1))
  fi
done

echo
if [[ "$failed" -eq 0 ]]; then
  echo "ALL INTEGRATION TESTS PASSED"
  exit 0
fi
echo "$failed suite(s) FAILED" >&2
exit 4
