#!/bin/sh
# Evolution items (EvolutionItemModifier.apply) against the pinned upstream, one seed per call.
# usage: tools/itemevodiff.sh <upstream checkout> [N=2000] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/item_evolution_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/item-evolution.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_IEVO_OUT:-$BUILD/game-item-evolution-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_IEVO_N="$N" GC_IEVO_SEED="$SEED" GC_IEVO_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/item-evolution.producer.test.ts)
"$BUILD/item_evolution_test" "$TRACE"
