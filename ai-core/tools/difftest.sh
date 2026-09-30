#!/bin/sh
# Generate N upstream cases and replay them through ai-core.
# Usage: tools/difftest.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
# AITEST_WHY=<reason> lists the cases a reason left out; AITEST_STATE=1 prints a failing case's state.
set -e
UP=${1:?upstream dir}; N=${2:-5000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/ai-core-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/ai.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && AI_SEED=$SEED AI_N=$N AI_OUT=$BUILD/ai-cases.txt npx vitest run test/tests/zz-dump/ai.producer.test.ts >/dev/null)
"$BUILD/aitest" "$BUILD/ai-cases.txt"
