#!/bin/sh
# Clowning Around (src/gc_clown.c: the fight, the ability swap offer and its outro, the item shuffle, the type shuffle) against the pinned upstream: the real encounter at waves 80..180 on six party layouts with random species, fusions, moves and
# held items of every tier; onInit's ability and types, the fight's trainer, party, moves, start effects and win, the swap offer, the item shuffle (Berries, Ultra and Rogue items made again from the pools) and the type shuffle, the run RNG around each step,
# compared by tests/mystery_clown_diff_test.c.
# usage: tools/clowndiff.sh <upstream checkout> [N=300] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-300}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_clown_diff_test"
mkdir -p "$UP/test/tests/zz-dump" "$BUILD"
cp "$HERE/tests/mystery-clown.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/clown-$SEED.txt}
rm -f "${TRACE%.txt}-clown.txt"
(cd "$UP" && GC_CLN_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-clown.producer.test.ts)
"$BUILD/mystery_clown_diff_test" "${TRACE%.txt}-clown.txt"
