#!/usr/bin/env bash
# Compile good/bad sources; run binary; symbols
source "$(dirname "$0")/common.sh"

echo "== test_compile_run =="
work="$TMPBASE/compile"
mkdir -p "$work"
cd "$work"
"$GHAR" init >/dev/null

cat > ok.cpp <<'EOF'
#include <cstdio>
int add(int a, int b) { return a + b; }
int main() { std::printf("%d\n", add(2, 3)); return 0; }
EOF

cat > bad.cpp <<'EOF'
int main() { return undeclared_symbol_xyz; }
EOF

assert_exit 0 "$GHAR" compile ok.cpp --name ok_build
assert_exit 4 "$GHAR" compile bad.cpp --name bad_build
assert_exit 4 "$GHAR" compile missing.cpp --name miss

bin="./.ghar/bin_ok_build"
assert_exit 0 "$GHAR" run --name ok_run -- "$bin"
assert_exit 0 "$GHAR" symbols add main --bin "$bin" --name syms
assert_exit 4 "$GHAR" symbols totally_fake_api_fn --bin "$bin" --name fake_sym

summary test_compile_run
