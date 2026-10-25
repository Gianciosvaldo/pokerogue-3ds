#!/bin/sh
# The Global Trade System (src/gc_gts.c) against the pinned upstream: the real encounter's onInit (three trade options per party
# member), the options' requirements and party filters, and the chosen option's own code (Check Trade Offers, Wonder Trade, Trade an
# Item, Leave), compared by tests/mystery_gts_diff_test.c.
# usage: tools/gtsdiff.sh <upstream checkout> [N=600] [seed=777]   (~0.13 s per case)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_gts_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-gts.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_GTS_OUT:-$BUILD/gts-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_GTS_N="$N" GC_GTS_SEED="$SEED" GC_GTS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-gts.producer.test.ts)
"$BUILD/mystery_gts_diff_test" "$TRACE"
