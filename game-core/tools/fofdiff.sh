#!/bin/sh
# Fight or Flight (src/gc_fof.c) against the pinned upstream: the real encounter's three options on random parties (stealing moves, luck,
# items, fainted members, levels, waves, biomes), compared by tests/mystery_fof_diff_test.c: onInit's boss and item, the rewards each option
# sets, the boss that enters the battle (new id, stats, moves, items, segments), its post-summon stat boost, the experience of a steal
# and the run RNG.
# usage: tools/fofdiff.sh <upstream checkout> [N=400] [seed=777]   (~0.05 s per case)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_fof_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-fightorflight.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/fof-$SEED.txt}
rm -f "${TRACE%.txt}-fightorflight.txt"
(cd "$UP" && GC_FOF_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-fightorflight.producer.test.ts)
"$BUILD/mystery_fof_diff_test" "${TRACE%.txt}-fightorflight.txt"
