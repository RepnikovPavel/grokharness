## Benchmark: OpenMMLab static scan

We automatically scan curated [OpenMMLab](https://github.com/open-mmlab) repositories with **ghar**
programmatic oracles (AST syntax + full `torch.*` attribute resolve — **no LLM**).

| Metric | Latest run |
|--------|----------:|
| Repos | **5** ([org](https://github.com/open-mmlab)) |
| Python files scanned | **1480** |
| Torch attr refs checked | **5353** |
| Syntax failures | **0** |
| Findings total | **67** |
| → **api_missing** (stock torch) | **6** |
| → accel backends (npu/mlu/musa) | **56** |
| → build flavor (ROCm/HIP) | **5** |
| Host torch | `2.6.0+cu124` |
| Report date (UTC) | 2026-07-18T11:26:38Z |

| Repo | Files | Syntax fail | Torch files | Attr refs | api_missing | accel | Findings |
|------|------:|------------:|------------:|----------:|------------:|------:|---------:|
| `mmengine` | 150 | 0 | 82 | 448 | 2 | 45 | 50 |
| `mmcv` | 226 | 0 | 149 | 966 | 3 | 11 | 16 |
| `mmpose` | 303 | 0 | 140 | 933 | 0 | 0 | 0 |
| `mmdetection` | 603 | 0 | 377 | 2217 | 1 | 0 | 1 |
| `mmsegmentation` | 198 | 0 | 124 | 789 | 0 | 0 | 0 |

**Headline:** on torch `2.6.0+cu124`, ghar flags **6** unresolved stock-API references (e.g. `SyncBatchNorm2d`, `PoolDataLoader`) plus **56** vendor-device symbols expected missing on CUDA wheels.

Full report: [`benchmarks/openmmlab/OPENMMLAB_SCAN.md`](benchmarks/openmmlab/OPENMMLAB_SCAN.md) · raw TSV: [`benchmarks/openmmlab/openmmlab_findings.tsv`](benchmarks/openmmlab/openmmlab_findings.tsv)

```sh
bash benchmarks/run_openmmlab_scan.sh
```

*More ecosystems next — this is the first public “scan a famous org” pillar.*
