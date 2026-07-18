# Bugs found and fixed (audit)

| ID | Severity | Bug | Fix |
|----|----------|-----|-----|
| B1 | High | `metrics` blob split on raw `;`/`=` → corrupt claims when feedback contains them | `metric_escape` / decode with escapes in `store.cpp` |
| B2 | High | Empty `.ghar/results.tsv` → first data row treated as header | `append_tsv_row` writes header if file empty |
| B3 | High | `import` interpolated module into Python `-c` string (injection) | argv `sys.argv[1]` + identifier validation |
| B4 | Medium | `kill(pid)` on timeout leaves `sh -c` children | `setpgid` + `kill(-pid, SIGKILL)` |
| B5 | Medium | `ghar verify` wiped **all** claims (bench/assert lost) | only remove `step_*` + `pipeline` |
| B6 | Medium | `verify --step` counted as `work verify_ok` (quota cheat) | count only full pipeline |
| B7 | Low | `status=error` + `EXIT_USAGE` returned as IO (3) | preserve usage/tool codes in `finish_check` |
| B8 | Low | double-escape of feedback via `escape_tsv` before metrics | store raw; encode layers escape |
| B9 | Low | missing portable includes (`cctype`, `signal.h`) | added |
| B10 | Low | `parse_int` no ERANGE / INT range check | reject out-of-range |

Regression: `tests/test_bugs.sh` (ctest label `integration`).

## Still open / not fixed this pass

| Item | Notes |
|------|--------|
| `assert --op le` + `--relative` | no-op branch (`+0`); relative only for eq/approx |
| Population stddev in bench | uses `/n` not `/n-1` (document, not wrong for CV) |
| `sha256_hex` name | FNV-1a, not SHA-256 — rename later |
| Concurrent writers to claims.tsv | no file lock |
