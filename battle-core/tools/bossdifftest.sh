#!/bin/sh
# Differential test for boss shield damage against pinned upstream behavior.
# Usage: tools/bossdifftest.sh <upstream dir> [N] [seed]
set -e
UP=${1:?upstream dir}; N=${2:-10000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/prcore-boss-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$HERE/tests/bosssegment.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && BOSSSEG_SEED=$SEED BOSSSEG_N=$N BOSSSEG_OUT=$BUILD/bosssegments.txt \
  npx vitest run test/tests/zz-dump/bosssegment.producer.test.ts >/dev/null)
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/bosssegmenttest"
"$BUILD/bosssegmenttest" "$BUILD/bosssegments.txt"
