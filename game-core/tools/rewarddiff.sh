#!/bin/sh
# Generate N upstream reward-screen cases and replay them through game-core's reward adapter.
# Usage: tools/rewarddiff.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
set -e
UP=${1:?upstream dir}; N=${2:-1000}; SEED=${3:-12345}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/reward_diff_test"
cp "$HERE/tests/reward.producer.test.ts" "$UP/test/tests/zz-dump/gc-reward.producer.test.ts"
(cd "$UP" && RD_SEED=$SEED RD_N=$N RD_OUT="$BUILD/gc-reward-$SEED.txt" npx vitest run --no-cache test/tests/zz-dump/gc-reward.producer.test.ts >/dev/null)
"$BUILD/reward_diff_test" "$BUILD/gc-reward-$SEED.txt"
