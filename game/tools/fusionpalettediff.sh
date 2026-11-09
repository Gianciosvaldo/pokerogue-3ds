#!/bin/sh
# source/fusion_palette.c (a fused Pokemon's sprite colours) against the pinned upstream's real Pokemon.updateFusionPalette: random sprite
# frames, shiny colour maps and the sprite shader's blend, run in the upstream test framework (tests/fusion-palette.producer.test.ts), then
# replayed through tests/fusion_palette_test.c.
# usage: tools/fusionpalettediff.sh <upstream checkout> [N=300] [seed=7]
# FP_OUT=<path> keeps the trace (gzip it into tests/fixtures/fusion-palette.native.txt.gz to refresh the committed one).
set -eu
UP=${1:?pinned upstream dir}; N=${2:-300}; SEED=${3:-7}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -C "$HERE/tests" fusion_palette_test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/fusion-palette.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${FP_OUT:-$HERE/tests/build/fusion-palette-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && FP_N="$N" FP_SEED="$SEED" FP_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/fusion-palette.producer.test.ts)
"$HERE/tests/build/fusion_palette_test" "$TRACE"
