#!/bin/sh
# The mystery encounters' selection rules (tier, per-run cap, wave range, party size, money, wave modulus, disallowed challenges, and the biome each can spawn in)
# against the pinned upstream: src/gc_mystery.c's table and biome function, compared by tests/mystery_rules_diff_test.c.
# usage: tools/rulesdiff.sh <upstream checkout>
set -eu
UP=${1:?pinned upstream dir}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_rules_diff_test"
mkdir -p "$UP/test/tests/zz-dump" "$BUILD"
cp "$HERE/tests/mystery-rules.producer.test.ts" "$UP/test/tests/zz-dump/"
OUT=${GC_RULES_OUT:-$BUILD/mystery-rules.txt}
(cd "$UP" && GC_RULES_OUT="$OUT" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-rules.producer.test.ts)
"$BUILD/mystery_rules_diff_test" "$OUT"
