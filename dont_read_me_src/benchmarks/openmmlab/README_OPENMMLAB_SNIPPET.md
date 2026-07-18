## Benchmark: OpenMMLab static scan

We automatically scan curated [OpenMMLab](https://github.com/open-mmlab) repositories with **ghar**
programmatic oracles (AST syntax + full `torch.*` attribute resolve — **no LLM**).

| Metric | Latest run |
|--------|----------:|
| Repos | **53** ([org](https://github.com/open-mmlab)) |
| Python files scanned | **7302** |
| Torch attr refs checked | **0** |
| Syntax failures | **2** |
| Findings total | **2** |
| → **api_missing** (stock torch) | **0** |
| → accel backends (npu/mlu/musa) | **0** |
| → build flavor (ROCm/HIP) | **0** |
| Host torch | `n/a` |
| Report date (UTC) | 2026-07-18T17:57:20Z |

| Repo | Files | Syntax fail | Torch files | Attr refs | api_missing | accel | Findings |
|------|------:|------------:|------------:|----------:|------------:|------:|---------:|
| `Amphion` | 637 | 0 | 0 | 0 | 0 | 0 | 0 |
| `AnyControl` | 117 | 0 | 0 | 0 | 0 | 0 | 0 |
| `awesome-vit` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `cocoapi` | 12 | 0 | 0 | 0 | 0 | 0 | 0 |
| `denseflow` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `ecosystem` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `FaceShot` | 294 | 0 | 0 | 0 | 0 | 0 | 0 |
| `FoleyCrafter` | 31 | 0 | 0 | 0 | 0 | 0 | 0 |
| `labelbee` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `labelbee-client` | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| `Live2Diff` | 41 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mdformat-openmmlab` | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mim` | 33 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mim-example` | 29 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmaction` | 181 | 1 | 0 | 0 | 0 | 0 | 1 |
| `mmaction2` | 220 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmagic` | 544 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmcv` | 128 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmdeploy` | 360 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmdetection` | 603 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmdetection3d` | 323 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmengine` | 150 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmengine-template` | 28 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmeval` | 86 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmfashion` | 161 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmfewshot` | 118 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmflow` | 93 | 0 | 0 | 0 | 0 | 0 | 0 |
| `MMGEN-FaceStylor` | 55 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmgeneration` | 146 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmhuman3d` | 233 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmocr` | 248 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmpose` | 303 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmpose-webcam-demo` | 66 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmpretrain` | 455 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmrazor` | 328 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmrotate` | 122 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmsegmentation` | 198 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmselfsup` | 144 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmskeleton` | 95 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmstyles` | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmtracking` | 137 | 0 | 0 | 0 | 0 | 0 | 0 |
| `mmyolo` | 96 | 0 | 0 | 0 | 0 | 0 | 0 |
| `Multimodal-GPT` | 37 | 0 | 0 | 0 | 0 | 0 | 0 |
| `OpenMMLabCamp` | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| `OpenMMLabCourse` | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| `OpenPCDet` | 180 | 1 | 0 | 0 | 0 | 0 | 1 |
| `OpenUnReID` | 85 | 0 | 0 | 0 | 0 | 0 | 0 |
| `PIA` | 19 | 0 | 0 | 0 | 0 | 0 | 0 |
| `playground` | 105 | 0 | 0 | 0 | 0 | 0 | 0 |
| `PowerPaint` | 11 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pre-commit-hooks` | 14 | 0 | 0 | 0 | 0 | 0 | 0 |
| `pytorch_sphinx_theme` | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| `StyleShot` | 19 | 0 | 0 | 0 | 0 | 0 | 0 |

**Headline:** on torch `n/a`, ghar flags **0** unresolved stock-API references (e.g. `SyncBatchNorm2d`, `PoolDataLoader`) plus **0** vendor-device symbols expected missing on CUDA wheels.

Full report: [`benchmarks/openmmlab/OPENMMLAB_SCAN.md`](benchmarks/openmmlab/OPENMMLAB_SCAN.md) · raw TSV: [`benchmarks/openmmlab/openmmlab_findings.tsv`](benchmarks/openmmlab/openmmlab_findings.tsv)

```sh
bash benchmarks/run_openmmlab_scan.sh
```

*More ecosystems next — this is the first public “scan a famous org” pillar.*
