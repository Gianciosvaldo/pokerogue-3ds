#!/bin/sh
# Differential test for Challenge Mode validation against pinned upstream behavior.
# Usage: tools/challengedifftest.sh <upstream dir> [N] [seed]
set -e
. "$(dirname "$0")/require-node22.sh"
UP=${1:?upstream dir}; N=${2:-10000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/prrun-challenge-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$HERE/tests/challenge.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && CHALLENGE_SEED=$SEED CHALLENGE_N=$N CHALLENGE_OUT=$BUILD/challenges.txt \
  npx vitest run test/tests/zz-dump/challenge.producer.test.ts >/dev/null)
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/challengetest"
"$BUILD/challengetest" "$BUILD/challenges.txt"
