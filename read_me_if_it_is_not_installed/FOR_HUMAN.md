# Install ghar (humans)

## Dependencies

- Linux
- `g++` ≥ 9 or `clang++` ≥ 9
- `cmake` ≥ 3.10
- Optional for full checks: CUDA toolkit (`nvcc`), `nvidia-smi`, `python3`, `pytest`, `binutils` (`nm`)

## From a clone

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
ghar --version
ghar doctor --format
```

## One-liner

```sh
curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/grokharness/main/read_me_if_it_is_not_installed/install.sh | sh
```

## Sanity

```sh
ghar init
ghar doctor --format
echo 'int main(){return 0;}' > /tmp/t.cpp
ghar compile /tmp/t.cpp --name t --format
ghar gate --format
```
