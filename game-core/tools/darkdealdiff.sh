#!/bin/sh
# Dark Deal (src/gc_darkdeal.c) against the pinned upstream: the real encounter's two options on random parties (items, fainted
# members, levels, balls, waves), compared by tests/mystery_darkdeal_diff_test.c: who leaves the party and with which items, the
# Rogue Balls, the boss (species, level, IVs, nature, moves, held items, segments), the battle's mode and the run RNG.
# usage: tools/darkdealdiff.sh <upstream checkout> [N=400] [seed=777]   (~0.04 s per case)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_darkdeal_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-darkdeal.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/darkdeal-$SEED.txt}
rm -f "${TRACE%.txt}-darkdeal.txt"
(cd "$UP" && GC_DD_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-darkdeal.producer.test.ts)
"$BUILD/mystery_darkdeal_diff_test" "${TRACE%.txt}-darkdeal.txt"
