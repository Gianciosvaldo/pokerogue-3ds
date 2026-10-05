#!/bin/sh
set -eu
. "$(dirname "$0")/require-node22.sh"
UP=${1:?upstream checkout};N=${2:-2000};SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/run-core-moveset}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/moveset.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DIFF_MOVESET=1 DIFF_N="$N" DIFF_SEED="$SEED" DIFF_OUT="$BUILD/moveset-$SEED.txt" \
 node node_modules/vitest/vitest.mjs run test/tests/zz-dump/moveset.producer.test.ts)
"$BUILD/movesettest" "$BUILD/moveset-$SEED.txt" "$N" "$SEED"
