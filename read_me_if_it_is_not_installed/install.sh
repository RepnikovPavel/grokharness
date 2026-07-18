#!/usr/bin/env sh
# install.sh — build and install ghar system-wide.
#
#   curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/grokharness/main/read_me_if_it_is_not_installed/install.sh | sh
#   sh read_me_if_it_is_not_installed/install.sh
#
set -eu

REPO="https://github.com/RepnikovPavel/grokharness.git"
PREFIX="/usr/local"
BINDIR="$PREFIX/bin"
BRANCH="${GHAR_BRANCH:-main}"

msg() { printf '==> %s\n' "$*"; }
step() { printf '\n[%s] %s\n' "$1" "$2"; }
err() { printf '==> ERROR: %s\n' "$*" >&2; }
die() { err "$*"; exit 1; }

STEP=0
next_step() { STEP=$((STEP + 1)); step "$STEP" "$1"; }

SCRIPT_DIR=$(cd "$(dirname "$0")" 2>/dev/null && pwd) || SCRIPT_DIR=""
if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/../CMakeLists.txt" ]; then
    SRC_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
    msg "Building from checkout: $SRC_DIR"
else
    command -v git >/dev/null 2>&1 || die "git is required for remote install."
    SRC_DIR=$(mktemp -d)
    msg "Temp build dir: $SRC_DIR"
    trap 'rm -rf "$SRC_DIR"' EXIT
    next_step "Cloning $REPO (branch $BRANCH)"
    git clone --progress --depth 1 --branch "$BRANCH" "$REPO" "$SRC_DIR" || die "git clone failed"
fi

[ -f "$SRC_DIR/CMakeLists.txt" ] || die "CMakeLists.txt not found in $SRC_DIR"

next_step "Checking build tools"
command -v cmake >/dev/null 2>&1 || die "cmake is required (apt: build-essential cmake)"

CXX="${CXX:-}"
[ -z "$CXX" ] && { command -v g++ >/dev/null 2>&1 && CXX=g++; }
[ -z "$CXX" ] && { command -v clang++ >/dev/null 2>&1 && CXX=clang++; }
[ -n "$CXX" ] || die "No C++17 compiler (need g++ >= 9 or clang++ >= 9)"

msg "cmake: $(cmake --version | head -n1)"
msg "compiler: $($CXX --version | head -n1)"

BUILD_DIR="$SRC_DIR/build"
JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)

next_step "CMake configure (Release)"
cmake -S "$SRC_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

next_step "Compiling ghar ($JOBS parallel jobs)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

BIN="$BUILD_DIR/ghar"
[ -x "$BIN" ] || die "Build did not produce $BIN"

next_step "Smoke test"
"$BIN" --version

writable_without_root() {
    [ "$(id -u)" -eq 0 ] && return 0
    if [ -d "$BINDIR" ]; then
        [ -w "$BINDIR" ] && return 0
        return 1
    fi
    parent="$BINDIR"
    while [ "$parent" != "/" ] && [ ! -d "$parent" ]; do
        parent=$(dirname "$parent")
    done
    [ -w "$parent" ]
}

next_step "Installing to $BINDIR/ghar"
if writable_without_root; then
    mkdir -p "$BINDIR"
    install -m 0755 "$BIN" "$BINDIR/ghar"
else
    msg "Need root for $BINDIR (running sudo)..."
    sudo sh -c "mkdir -p '$BINDIR' && install -m 0755 '$BIN' '$BINDIR/ghar'"
fi

"$BINDIR/ghar" --version
if command -v ghar >/dev/null 2>&1; then
    msg "Done. ghar is on PATH: $(command -v ghar)"
else
    msg "Done. Binary: $BINDIR/ghar (add $BINDIR to PATH if needed)"
fi
