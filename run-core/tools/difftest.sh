#!/bin/sh
# Generate N upstream cases and replay them through run-core.
# Usage: tools/difftest.sh <upstream dir> [N] [seed] [wave|shop|all]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares.
set -e
UP=${1:?upstream dir}; N=${2:-20000}; SEED=${3:-12345}; WHAT=${4:-all}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/run-core-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
for t in wave shop; do
  [ "$WHAT" = all ] || [ "$WHAT" = $t ] || continue
  cp "$HERE/tests/$t.producer.test.ts" "$UP/test/tests/zz-dump/"
  (cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT=$BUILD/$t-cases.txt npx vitest run test/tests/zz-dump/$t.producer.test.ts >/dev/null)
  echo "== $t"
  "$BUILD/${t}test" "$BUILD/$t-cases.txt"
done
