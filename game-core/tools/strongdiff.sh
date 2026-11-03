#!/bin/sh
# The Strong Stuff (src/gc_strong.c) against the pinned upstream: the real encounter's two options on random parties (species with equal base stat totals, fainted members, levels, items), compared by tests/mystery_strong_diff_test.c: onInit's config, the juice every Pokemon gets and its stats, the fight (boss, five bars, moves, items, run RNG, start-of-battle Gastro Acid and Stealth Rock, the post-summon Def and Sp. Def boost, rewards).
# usage: tools/strongdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_strong_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-strong.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/strong-$SEED.txt}
rm -f "${TRACE%.txt}-strong.txt"
(cd "$UP" && GC_STR_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-strong.producer.test.ts)
"$BUILD/mystery_strong_diff_test" "${TRACE%.txt}-strong.txt"
