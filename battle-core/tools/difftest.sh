#!/bin/sh
# Generate N upstream cases and replay them through the C port.
# Usage: tools/difftest.sh <upstream dir> [N] [seed]
# DIFF_ABILITIES=0 and DIFF_FOCUS=1 in the environment reach the producer.
set -e
UP=${1:?upstream dir}; N=${2:-5000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/prcore-build}
mkdir -p "$BUILD"
cp "$HERE/tests/difftest.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT=$BUILD/cases.txt npx vitest run test/tests/zz-dump/difftest.producer.test.ts >/dev/null)
make -s -C "$HERE" BUILD="$BUILD"
"$BUILD/difftest" "$BUILD/cases.txt"
