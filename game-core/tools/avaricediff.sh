#!/bin/sh
# Absolute Avarice (src/gc_avarice.c) against the pinned upstream: the real encounter's three options on random parties with random Berries, compared by tests/mystery_avarice_diff_test.c: onInit's boss config and the Berries it takes, the fight (boss, Berries it holds, start-of-battle Stuff Cheeks, rewards, the Reviver Seed callback, the post-summon stat changes, run RNG), the plea (the Berries given back) and the feast (the Greedent that is caught).
# usage: tools/avaricediff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_avarice_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-avarice.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/avarice-$SEED.txt}
rm -f "${TRACE%.txt}-avarice.txt"
(cd "$UP" && GC_AVA_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-avarice.producer.test.ts)
"$BUILD/mystery_avarice_diff_test" "${TRACE%.txt}-avarice.txt"
