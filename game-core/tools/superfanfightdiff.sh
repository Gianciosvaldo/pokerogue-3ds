#!/bin/sh
# The Bug-Type Superfan's fight (option 1, src/gc_superfan.c) against the pinned upstream: the real encounter at waves 30..180 on four party layouts, the team by wave band (species pools, the shuffled third pool,
# Bug-type boss and form changes, party strengths, levels, moves, abilities, natures, items), the four tutor moves drawn on the option stream, the reward flags and the run RNG after the option,
# compared by tests/mystery_superfan_fight_diff_test.c.
# usage: tools/superfanfightdiff.sh <upstream checkout> [N=350] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-350}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_superfan_fight_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-superfan-fight.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/superfanfight-$SEED.txt}
rm -f "${TRACE%.txt}-superfanfight.txt"
(cd "$UP" && GC_SF_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-superfan-fight.producer.test.ts)
"$BUILD/mystery_superfan_fight_diff_test" "${TRACE%.txt}-superfanfight.txt"
