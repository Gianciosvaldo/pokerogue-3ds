#!/bin/sh
# Generate N upstream wave cases and replay them through run-core.
# Usage: tools/difftest.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
set -e
UP=${1:?upstream dir}; N=${2:-20000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/run-core-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$HERE/tests/wave.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT=$BUILD/wave-cases.txt npx vitest run test/tests/zz-dump/wave.producer.test.ts >/dev/null)
make -s -C "$HERE" BUILD="$BUILD"
"$BUILD/wavetest" "$BUILD/wave-cases.txt"
