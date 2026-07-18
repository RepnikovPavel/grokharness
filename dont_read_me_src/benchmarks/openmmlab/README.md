# OpenMMLab org static scan

Scans **all public [open-mmlab](https://github.com/open-mmlab) repos** listed in `repos.tsv`
(~50+, generated from GitHub API) with `oracles/repo_scan.py` via:

```sh
bash benchmarks/run_openmmlab_scan.sh
# optional: OPENMMLAB_CACHE=/path OUT_DIR=/path NO_TORCH=1
```

- **No LLM judge** — AST syntax + optional torch attr resolve.
- Shallow clones into `$OPENMMLAB_CACHE` (default `results/openmmlab_cache`).
- Server layout: `/mnt/hdd1/grokharness_data/{repo,ghar_results,grokharness}`.

See latest `OPENMMLAB_SCAN.md` and `openmmlab_summary.tsv` in this directory.
