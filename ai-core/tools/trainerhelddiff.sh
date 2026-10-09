#!/bin/sh
# Additive Card 108 suite; established AI producer/comparator are unchanged.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-"$HERE/build"}
[ "$(node -p 'process.versions.node.split(".")[0]')" = 22 ] || { echo 'Node 22 required' >&2; exit 2; }
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'wrong upstream pin' >&2; exit 2; }
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
BUILD=$(cd "$BUILD" && pwd)
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/trainerheldtest"
cp "$HERE/tests/trainer-held.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && AI108_N="$N" AI108_SEED="$SEED" AI108_OUT="$BUILD/ai108-$SEED.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/trainer-held.producer.test.ts --maxWorkers=1)
"$BUILD/trainerheldtest" "$BUILD/ai108-$SEED.txt" "$N"
