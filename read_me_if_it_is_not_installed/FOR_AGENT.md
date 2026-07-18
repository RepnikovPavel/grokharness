# Installing ghar — for AI agents

If `command -v ghar` fails:

```sh
curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/grokharness/main/read_me_if_it_is_not_installed/install.sh | sh
```

Or from a checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)"
install -m 0755 build/ghar "$HOME/.local/bin/ghar"
```

Requires: `git`, `g++` or `clang++`, `cmake`.

Verify:

```sh
ghar --version
ghar scaffold
ghar config
ghar verify    # MUST exit 0 before user delivery
```

Read `prompt.txt` and `AGENTS.md`. Primary contract: **`ghar verify`**, not LLM self-review.
