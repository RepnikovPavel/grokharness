# ghar local results UI

One local dashboard for **measured** ghar outputs:

| Page | Shows |
|------|--------|
| `/` | Counts + top actionable findings + model catches |
| `/findings` | Path / attr / kind (default: hide npu/mlu noise) |
| `/real-model` | Per-trial coding-model claims + ghar exit |
| `/hallucinations` | Synthetic + py/torch labeled suites |
| `/benchmarks` | Catalog of hallu-search suites |

## Run

```sh
# from repo root
docker compose up --build
# → http://127.0.0.1:8765/
```

Without Docker:

```sh
# optional: point at your suite outputs
export GHAR_RESULTS="$(pwd)/results"
python3 ui/app.py
```

If `results/` has no TSVs, the app uses `ui/sample_results/` so the UI is never blank.

## Tests

```sh
bash tests/test_ui_http.sh    # real HTTP against sample_results
bash tests/test_ui_data.sh    # loaders against GHAR_RESULTS (defaults to results/)
```
