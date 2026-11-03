#!/bin/sh
# The starter rules of the Endless modes (the cost limit of getRunValueLimit, SelectStarterPhase.initBattle's party and a Spliced Endless
# starter's generateFusionSpecies(true)) against the pinned upstream, one seed per call.
# usage: tools/startersdiff.sh <upstream checkout> [N=600] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/starters_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/starters.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_STARTERS_OUT:-$BUILD/starters-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_STARTERS_N="$N" GC_STARTERS_SEED="$SEED" GC_STARTERS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/starters.producer.test.ts)
"$BUILD/starters_diff_test" "$TRACE"
