#!/bin/sh
# Bounded vitamin selection/application/purchase-boundary oracle, Node22 only.
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-vitamin-build}
case "$(node --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2 ;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/vitamin.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/vitamin-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && VIT_N="$N" VIT_SEED="$SEED" VIT_OUT="$TRACE" \
  node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/vitamin.producer.test.ts \
  >"$BUILD/vitamin-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/vitamin-$SEED.log" >&2; exit 2; }
"$BUILD/vitamintest" "$TRACE"
