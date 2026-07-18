# OpenMMLab scan pillar

Automatically clone + statically scan curated OpenMMLab packages with ghar oracles.

| Artifact | Role |
|----------|------|
| `repos.tsv` | which repos / refs / package globs |
| `OPENMMLAB_SCAN.md` | human report (committed snapshot) |
| `openmmlab_findings.tsv` | every finding row |
| `openmmlab_summary.tsv` | per-repo counters |
| `README_OPENMMLAB_SNIPPET.md` | block mirrored on main README |

```sh
bash benchmarks/run_openmmlab_scan.sh
```

Shallow clones live under `results/openmmlab_cache/` (gitignored via `results/`).
Findings categories: `api_missing` | `accel_backend` | `build_flavor` | `syntax_error`.
