#!/bin/sh
# Generate N upstream cases and replay them through reward-core.
# Usage: tools/difftest.sh <upstream dir> [N] [seed] [rewards|victory|apply|all]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
set -e
UP=${1:?upstream dir}; N=${2:-3000}; SEED=${3:-12345}; WHAT=${4:-all}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/reward-core-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
for t in rewards victory apply; do
  [ "$WHAT" = all ] || [ "$WHAT" = $t ] || continue
  cp "$HERE/tests/$t.producer.test.ts" "$UP/test/tests/zz-dump/"
  (cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT=$BUILD/$t-cases.txt npx vitest run --no-cache test/tests/zz-dump/$t.producer.test.ts >/dev/null)
  echo "== $t"
  "$BUILD/${t}test" "$BUILD/$t-cases.txt"
done
