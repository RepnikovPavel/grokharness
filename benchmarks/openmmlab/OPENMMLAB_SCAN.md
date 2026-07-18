# ghar × OpenMMLab scan report

Generated: 2026-07-18T17:51:02Z → 2026-07-18T17:57:20Z (378s)  
Scanner: `oracles/repo_scan.py` + spot checks via `/mnt/hdd1/grokharness_data/grokharness/build/ghar`  
Installed torch: **n/a**  
Org: [open-mmlab](https://github.com/open-mmlab)

## What this measures (honest scope)

Programmatic static checks only — **no LLM judge**, no full training run:

| Check / category | Meaning |
|------------------|---------|
| **syntax_error** | `ast.parse` fails — real syntax bug or corrupt tree |
| **api_missing** | `torch.*` name does not exist on stock installed torch (API drift / typo / dead wrapper) |
| **accel_backend** | vendor device namespaces (`torch.npu` / `mlu` / `musa`) absent on CUDA wheels — **expected** |
| **build_flavor** | ROCm/HIP-only symbols on a CUDA build |

Not counted: missing `mmcv`/`mmdet` package imports, training correctness, CUDA kernel numerics.

## Scoreboard

| Metric | Value |
|--------|------:|
| repos scanned | **53** |
| Python files | **7302** |
| syntax failures | **2** |
| files with torch usage | **0** |
| torch attr references checked | **0** |
| findings total | **2** |
| of which **api_missing** | **0** |
| of which **accel_backend** | **0** |
| of which **build_flavor** | **0** |

### Per repository

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

### Findings by category

| Category | Count |
|----------|------:|
| `syntax_error` | 2 |

### api_missing attributes (interesting)

| Attr | Count |
|------|------:|
| — | 0 |

### Top unresolved attributes overall (host torch=n/a)

| Attr | Count |
|------|------:|
| — | 0 |

### Example findings (api_missing first)

| Repo | Path | Category | Attr/Line | Detail |
|------|------|----------|-----------|--------|
| `mmaction` | `configs/TSN/tsn_kinetics400_2d_rgb_r50_seg3_f1s1.py` | `syntax_error` | `110` | closing parenthesis ')' does not match opening parenthesis '[' |
| `OpenPCDet` | `pcdet/datasets/kitti/kitti_object_eval_python/evaluate.py` | `syntax_error` | `5` | invalid syntax |

## Reproduce

```sh
cmake -S . -B build && cmake --build build -j$(nproc)
bash benchmarks/run_openmmlab_scan.sh
# → results/OPENMMLAB_SCAN.md
# → results/openmmlab_findings.tsv
```

Cache: `results/openmmlab_cache/` (shallow clones).  
Config: `benchmarks/openmmlab/repos.tsv`.
