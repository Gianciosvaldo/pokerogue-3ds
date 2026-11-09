#!/bin/sh
# The Safari Zone (src/gc_safari.c) against the pinned upstream: the real encounter's entry and its four actions (ball, bait, mud, run) on random parties, biomes, waves, money and
# Shiny / Ability Charms, compared by tests/mystery_safari_diff_test.c step by step: the run RNG, the Pokemon summoned, the catch and flee stages, the ball's rolls, the texts said,
# the cursor and the actions handed back, and the way out.
# usage: tools/safaridiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_safari_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-safari.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/safari-$SEED.txt}
rm -f "${TRACE%.txt}-safari.txt"
(cd "$UP" && GC_SAF_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-safari.producer.test.ts)
"$BUILD/mystery_safari_diff_test" "${TRACE%.txt}-safari.txt"
