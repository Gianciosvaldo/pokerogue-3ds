#!/bin/sh
# Biome weather and terrain (Arena.resetArenaEffects, setBiomeWeather, setBiomeTerrain) against the pinned upstream,
# one seed per call. usage: tools/biomefielddiff.sh <upstream checkout> [N=3000] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-3000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/biome_fields_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/biome-fields.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_BF_OUT:-$BUILD/game-biome-fields-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_BF_N="$N" GC_BF_SEED="$SEED" GC_BF_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/biome-fields.producer.test.ts)
"$BUILD/biome_fields_diff_test" "$TRACE"
