#!/bin/sh
# Delibird-y (src/gc_delibirdy.c) against the pinned upstream: the real encounter's requirements, the options' meetsRequirements(), the party
# screen's filter and item list, and the chosen option's own code (Give Money, Give Food, Give an Item) on random parties holding items of every
# kind, compared by tests/mystery_delibirdy_diff_test.c.
# usage: tools/delibirddiff.sh <upstream checkout> [N=600] [seed=777]   (~0.1 s per case)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_delibirdy_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-delibirdy.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_DELI_OUT:-$BUILD/delibirdy-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_DELI_N="$N" GC_DELI_SEED="$SEED" GC_DELI_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-delibirdy.producer.test.ts)
"$BUILD/mystery_delibirdy_diff_test" "$TRACE"
