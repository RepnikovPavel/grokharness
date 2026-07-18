# ghar × OpenMMLab scan report

Generated: 2026-07-18T11:26:20Z → 2026-07-18T11:26:38Z (18s)  
Scanner: `oracles/repo_scan.py` + spot checks via `/home/user/grokharness/build/ghar`  
Installed torch: **2.6.0+cu124**  
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
| repos scanned | **5** |
| Python files | **1480** |
| syntax failures | **0** |
| files with torch usage | **872** |
| torch attr references checked | **5353** |
| findings total | **67** |
| of which **api_missing** | **6** |
| of which **accel_backend** | **56** |
| of which **build_flavor** | **5** |

### Per repository

| Repo | Files | Syntax fail | Torch files | Attr refs | api_missing | accel | Findings |
|------|------:|------------:|------------:|----------:|------------:|------:|---------:|
| `mmengine` | 150 | 0 | 82 | 448 | 2 | 45 | 50 |
| `mmcv` | 226 | 0 | 149 | 966 | 3 | 11 | 16 |
| `mmpose` | 303 | 0 | 140 | 933 | 0 | 0 | 0 |
| `mmdetection` | 603 | 0 | 377 | 2217 | 1 | 0 | 1 |
| `mmsegmentation` | 198 | 0 | 124 | 789 | 0 | 0 | 0 |

### Findings by category

| Category | Count |
|----------|------:|
| `accel_backend` | 56 |
| `api_missing` | 6 |
| `build_flavor` | 5 |

### api_missing attributes (interesting)

| Attr | Count |
|------|------:|
| `torch.nn.SyncBatchNorm2d` | 2 |
| `torch.utils.data.PoolDataLoader` | 2 |
| `torch.distributed.group.WORLD` | 2 |

### Top unresolved attributes overall (host torch=2.6.0+cu124)

| Attr | Count |
|------|------:|
| `torch.musa` | 8 |
| `torch.npu` | 7 |
| `torch.mlu` | 6 |
| `torch.utils.cpp_extension.ROCM_HOME` | 3 |
| `torch.mlu.current_device` | 2 |
| `torch.npu.current_device` | 2 |
| `torch.nn.SyncBatchNorm2d` | 2 |
| `torch.utils.data.PoolDataLoader` | 2 |
| `torch.version.hip` | 2 |
| `torch.npu.set_device` | 2 |
| `torch.distributed.group.WORLD` | 2 |
| `torch.npu_deformable_conv2dbk` | 2 |
| `torch.musa.current_device` | 1 |
| `torch.npu.native_device` | 1 |
| `torch.musa.empty_cache` | 1 |

### Example findings (api_missing first)

| Repo | Path | Category | Attr/Line | Detail |
|------|------|----------|-----------|--------|
| `mmengine` | `mmengine/utils/dl_utils/parrots_wrapper.py` | `api_missing` | `torch.nn.SyncBatchNorm2d` | torch attr not found in installed torch: torch.nn.SyncBatchNorm2d |
| `mmengine` | `mmengine/utils/dl_utils/parrots_wrapper.py` | `api_missing` | `torch.utils.data.PoolDataLoader` | torch attr not found in installed torch: torch.utils.data.PoolDataLoader |
| `mmcv` | `mmcv/ops/sync_bn.py` | `api_missing` | `torch.distributed.group.WORLD` | torch attr not found in installed torch: torch.distributed.group.WORLD |
| `mmcv` | `mmcv/utils/parrots_wrapper.py` | `api_missing` | `torch.nn.SyncBatchNorm2d` | torch attr not found in installed torch: torch.nn.SyncBatchNorm2d |
| `mmcv` | `mmcv/utils/parrots_wrapper.py` | `api_missing` | `torch.utils.data.PoolDataLoader` | torch attr not found in installed torch: torch.utils.data.PoolDataLoader |
| `mmdetection` | `mmdet/utils/dist_utils.py` | `api_missing` | `torch.distributed.group.WORLD` | torch attr not found in installed torch: torch.distributed.group.WORLD |
| `mmengine` | `mmengine/utils/dl_utils/collect_env.py` | `build_flavor` | `torch.utils.cpp_extension.ROCM_HOME` | torch attr not found in installed torch: torch.utils.cpp_extension.ROCM_HOME |
| `mmengine` | `mmengine/utils/dl_utils/parrots_wrapper.py` | `build_flavor` | `torch.utils.cpp_extension.ROCM_HOME` | torch attr not found in installed torch: torch.utils.cpp_extension.ROCM_HOME |
| `mmengine` | `mmengine/utils/dl_utils/parrots_wrapper.py` | `build_flavor` | `torch.version.hip` | torch attr not found in installed torch: torch.version.hip |
| `mmcv` | `mmcv/utils/parrots_wrapper.py` | `build_flavor` | `torch.utils.cpp_extension.ROCM_HOME` | torch attr not found in installed torch: torch.utils.cpp_extension.ROCM_HOME |
| `mmcv` | `mmcv/utils/parrots_wrapper.py` | `build_flavor` | `torch.version.hip` | torch attr not found in installed torch: torch.version.hip |
| `mmengine` | `mmengine/model/base_model/data_preprocessor.py` | `accel_backend` | `torch.mlu` | torch attr not found in installed torch: torch.mlu |

## Reproduce

```sh
cmake -S . -B build && cmake --build build -j$(nproc)
bash benchmarks/run_openmmlab_scan.sh
# → results/OPENMMLAB_SCAN.md
# → results/openmmlab_findings.tsv
```

Cache: `results/openmmlab_cache/` (shallow clones).  
Config: `benchmarks/openmmlab/repos.tsv`.
