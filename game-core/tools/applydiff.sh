#!/bin/sh
# Generate N upstream reward-application cases (reward-core's own apply producer, unchanged) and replay them
# through game-core's gc_port_apply_reward on a real run.
# Usage: tools/applydiff.sh <upstream dir> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
set -e
UP=${1:?upstream dir}; N=${2:-4000}; SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/reward_apply_diff_test"
cp "$HERE/../reward-core/tests/apply.producer.test.ts" "$UP/test/tests/zz-dump/gc-apply.producer.test.ts"
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$BUILD/gc-apply-$SEED.txt" npx vitest run --no-cache test/tests/zz-dump/gc-apply.producer.test.ts >/dev/null)
"$BUILD/reward_apply_diff_test" "$BUILD/gc-apply-$SEED.txt"
