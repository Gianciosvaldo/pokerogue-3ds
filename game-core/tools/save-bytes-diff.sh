#!/bin/sh
# The saves of runs and profiles that never had a nickname must be the bytes the base wrote. Builds tools/savebytes.c against the base (a git ref,
# default origin/main, in a temporary worktree) and against this tree, plays RUNS runs (default 200) in each, and compares every save's length
# and hash (the run's and the system profile's). Exit 0 and "IDENTICAL" when they agree. From anywhere: sh game-core/tools/save-bytes-diff.sh [BASE_REF [RUNS]]
set -e
ROOT=$(git rev-parse --show-toplevel)
BASE=${1:-origin/main}
RUNS=${2:-200}
TMP=$(mktemp -d)
trap 'git -C "$ROOT" worktree remove --force "$TMP/base" >/dev/null 2>&1 || true; rm -rf "$TMP"' EXIT
git -C "$ROOT" worktree add -q --detach "$TMP/base" "$BASE"
build() {   # tree, output
    (cd "$1/game-core" &&
     make -s -j"$(nproc)" build/libgc.a build/reward-core/libreward.a build/ai-core/libai.a build/run-core/librun.a build/run-core/battle-core/libprcore.a >/dev/null &&
     cc -Iinclude -Isrc -I../run-core/include -I../ai-core/include -I../reward-core/include -I../reward-core/gen/out -I../battle-core/include \
        -I../battle-core/gen/out -O2 -std=c99 -w -ffp-contract=off "$ROOT/game-core/tools/savebytes.c" build/libgc.a build/reward-core/libreward.a \
        build/ai-core/libai.a build/run-core/librun.a build/run-core/battle-core/libprcore.a -lm -o "$2")
}
build "$TMP/base" "$TMP/base.exe"
build "$ROOT" "$TMP/this.exe"
"$TMP/base.exe" "$RUNS" > "$TMP/base.txt"
"$TMP/this.exe" "$RUNS" > "$TMP/this.txt"
echo "base ($BASE): $(tail -n1 "$TMP/base.txt"); this tree: $(tail -n1 "$TMP/this.txt"); refused starter lists: $(grep -c refused "$TMP/this.txt")"
cmp "$TMP/base.txt" "$TMP/this.txt" && echo IDENTICAL
