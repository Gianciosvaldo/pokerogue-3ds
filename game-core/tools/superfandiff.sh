#!/bin/sh
# The Bug-Type Superfan (src/gc_superfan.c) against the pinned upstream: the real encounter on seven party layouts (random subsets of at least three, from no Bug type to six; fainted members;
# Quick Claws, Grip Claws, Bug and other attack type boosters; the Mega Bracelet and the Dynamax Band) at waves 30..180: the requirements of the encounter and of each option, then option 2 (Show Your
# Bug-types: the reward by the number of Bug types, the special items by randSeedInt, the text) and option 3 (Gift a Bug Item: the party screen's filter for every member, the items offered, the item
# taken, the Golden Bug Net and the Reviver Seed as the reward), with the run RNG after each of the two phases, compared by tests/mystery_superfan_diff_test.c.
# usage: tools/superfandiff.sh <upstream checkout> [N=350] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-350}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_superfan_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-superfan.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/superfan-$SEED.txt}
rm -f "${TRACE%.txt}-superfan.txt"
(cd "$UP" && GC_SF_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-superfan.producer.test.ts)
"$BUILD/mystery_superfan_diff_test" "${TRACE%.txt}-superfan.txt"
