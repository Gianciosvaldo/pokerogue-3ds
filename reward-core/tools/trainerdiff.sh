#!/bin/sh
# Card 97: generate N upstream trainer reward cases for one seed and replay them through reward-core.
# Usage: tools/trainerdiff.sh <upstream dir> [N] [seed]. Needs Node 22 (docs/HANDOFF.md); the upstream checkout is
# the one battle-core/tools/setup-upstream.sh prepares. Prints the comparator's totals; exits with its status.
set -e
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-core-build}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required, found $(node --version)" >&2; exit 2 ;; esac
echo "node $(node --version)"
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/trainer-rewards.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/trainer-rewards-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$TRACE" npx vitest run --no-cache test/tests/zz-dump/trainer-rewards.producer.test.ts >"$BUILD/trainer-rewards-$SEED.log" 2>&1) \
  || { echo "producer failed (log: $BUILD/trainer-rewards-$SEED.log)" >&2; exit 2; }
"$BUILD/trainerrewardstest" "$TRACE"
