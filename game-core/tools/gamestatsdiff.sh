#!/bin/sh
# The Game Stats that setPokemonSeen / setPokemonSpeciesCaught keep (seen, caught, hatched, by rarity and shine, a fusion's half counted) against the
# pinned upstream: the trace of tests/gamestats.producer.test.ts, compared op by op by tests/gamestats_diff_test.c. One seed per call.
# usage: tools/gamestatsdiff.sh <upstream checkout> [N=2000] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/gamestats_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/gamestats.producer.test.ts" "$UP/test/tests/zz-dump/gc-gamestats.producer.test.ts"
TRACE=${GC_GS_OUT:-$BUILD/gamestats-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_GS_N="$N" GC_GS_SEED="$SEED" GC_GS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/gc-gamestats.producer.test.ts)
"$BUILD/gamestats_diff_test" "$TRACE"
