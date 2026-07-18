#!/usr/bin/env bash
# Integration: ghar python / torch / torch-attr validators
source "$(dirname "$0")/common.sh"

export GHAR_ROOT="$ROOT"
PY="$ROOT/dont_read_me_src/testdata/python"
TORCH="$ROOT/dont_read_me_src/testdata/torch"

echo "== test_python_torch =="
work="$TMPBASE/pytorch"
mkdir -p "$work"
cd "$work"
"$GHAR" init >/dev/null

# --- CLI present ---
help_out=$("$GHAR" --help 2>&1 || true)
assert_contains "$help_out" "python" "help mentions python"
assert_contains "$help_out" "torch" "help mentions torch"
ver=$("$GHAR" --version 2>&1)
assert_contains "$ver" "0.4" "version 0.4.x"

# --- Oracle script resolves ---
assert_exit 0 "$GHAR" python --file "$PY/ok_hello.py" --name py_ok
assert_exit 0 "$GHAR" python --file "$PY/ok_hello.py" --exec --name py_exec
assert_exit 4 "$GHAR" python --file "$PY/syntax_bad.py" --name py_syn
assert_exit 4 "$GHAR" python --file "$PY/import_missing.py" --name py_imp
assert_exit 4 "$GHAR" python --file "$PY/exec_error.py" --exec --name py_rt

# bare path form
assert_exit 0 "$GHAR" python "$PY/ok_hello.py" --exec --name py_bare

# --- Torch ---
if python3 -c "import torch" 2>/dev/null; then
  assert_exit 0 "$GHAR" torch-attr torch.nn.Linear torch.matmul --name ta_ok
  assert_exit 4 "$GHAR" torch-attr torch.nn.SuperDuperLayer --name ta_bad
  assert_exit 0 "$GHAR" torch --file "$TORCH/ok_linear.py" --forward --name t_ok
  assert_exit 4 "$GHAR" torch --file "$TORCH/hallucinated_attr.py" --strict-attrs --name t_hall
  # no parent-module fallback for torch.nn.functional.<fake>
  assert_exit 4 "$GHAR" torch --file "$TORCH/hallucinated_functional.py" --forward --strict-attrs --name t_func
  # import alias F.fake must be static-checked
  assert_exit 4 "$GHAR" torch --file "$TORCH/hallucinated_alias_F.py" --forward --strict-attrs --name t_alias
  assert_exit 0 "$GHAR" torch --file "$TORCH/ok_alias_F.py" --forward --strict-attrs --name t_alias_ok
  assert_exit 4 "$GHAR" torch --file "$TORCH/bad_forward.py" --forward --name t_fwd
  assert_exit 0 "$GHAR" torch "$TORCH/matmul_opt.py" --forward --name t_opt

  # gate: only ok claims after reset
  "$GHAR" reset >/dev/null
  assert_exit 0 "$GHAR" python --file "$PY/ok_hello.py" --exec --name gpy
  assert_exit 0 "$GHAR" torch --file "$TORCH/ok_linear.py" --forward --name gt
  assert_exit 0 "$GHAR" gate
else
  echo "  SKIP torch tests (torch not installed)"
  pass=$((pass + 1))
fi

# metrics in claims
claims=$("$GHAR" claims 2>&1 || true)
assert_contains "$claims" "python" "claims include python kind" || true

summary test_python_torch
