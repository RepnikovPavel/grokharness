# ghar local results UI

Shows **concrete** ghar results in a browser:

1. **Code findings** — kind, path, line/attr, detail  
2. **Coding-model hallu** — per-trial case id, claim, ghar exit, caught  
3. **Benchmarks list** — synthetic / py-torch / real-model / perf / code scan  

## Docker Compose (recommended)

From repo root (after you have run suites into `results/`):

```sh
docker compose up --build
# → http://127.0.0.1:8765/   (compose uses host networking + port 8765)
```

## Host (no Docker)

```sh
export GHAR_RESULTS="$(pwd)/results"
python3 ui/app.py
```

## API

| Path | Returns |
|------|---------|
| `/api/health` | status + dashboard stats |
| `/api/findings` | code findings JSON |
| `/api/real-model` | real_model_cases rows |
| `/api/hallucinations` | synthetic + py_torch cases |
| `/api/benchmarks` | benchmark catalog |

Data is read-only from `GHAR_RESULTS` (default: `./results`).
