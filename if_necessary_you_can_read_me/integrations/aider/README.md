# Aider + ghar

Industry pattern from https://aider.chat/docs/usage/lint-test.html :

```bash
# After each edit: lint/test via ghar pipeline (config: .ghar/config)
aider --lint-cmd 'ghar verify --no-gate --step lint' \
      --test-cmd  'ghar verify' \
      --auto-test
```

Or single oracle:

```bash
aider --test-cmd 'ghar verify' --auto-test
```

`ghar verify` runs lint→build→test from `.ghar/config`, prints FEEDBACK on failure,
then `ghar gate` (delivery contract).
