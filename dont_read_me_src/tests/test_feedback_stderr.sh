#!/usr/bin/env bash
# Agents rely on FEEDBACK on stderr for the fix loop (not silent fail).
source "$(dirname "$0")/common.sh"

echo "== test_feedback_stderr =="
work="$TMPBASE/feedback"
mkdir -p "$work"
cd "$work"
"$GHAR" init >/dev/null

# Intentional model-style hallucination: fake package
set +e
out=$("$GHAR" import totally_fake_agent_pkg_xyz --name fb_import 2>"$TMPBASE/fb_import.err")
ec=$?
set -e
assert_eq "$ec" "4" "import fake package exit 4"
err=$(cat "$TMPBASE/fb_import.err")
assert_contains "$err" "ghar FEEDBACK" "stderr has FEEDBACK banner"
assert_contains "$err" "totally_fake_agent_pkg_xyz" "stderr names the missing package"

# Bad C++ compile
cat > bad.cpp <<'EOF'
int main(){ return undeclared_symbol_agent_xyz; }
EOF
set +e
"$GHAR" compile bad.cpp --name fb_compile >/dev/null 2>"$TMPBASE/fb_compile.err"
ec2=$?
set -e
assert_eq "$ec2" "4" "compile bad exit 4"
err2=$(cat "$TMPBASE/fb_compile.err")
assert_contains "$err2" "ghar FEEDBACK" "compile FEEDBACK on stderr"
assert_contains "$err2" "undeclared_symbol_agent_xyz" "compile stderr has compiler detail"

# Gate FEEDBACK when failing claims exist
set +e
"$GHAR" gate >/dev/null 2>"$TMPBASE/fb_gate.err"
ec3=$?
set -e
assert_eq "$ec3" "4" "gate exit 4 with fails"
err3=$(cat "$TMPBASE/fb_gate.err")
assert_contains "$err3" "ghar FEEDBACK" "gate FEEDBACK on stderr"
assert_contains "$err3" "ghar reset" "gate hints reset for intentional fails"

summary test_feedback_stderr
