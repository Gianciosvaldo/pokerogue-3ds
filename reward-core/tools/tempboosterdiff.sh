#!/bin/sh
# Bounded temporary-booster acquisition/lapse and effect projection oracle, Node22 only.
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-tempbooster-build}
case "$(node --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2 ;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/temp-booster.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/tempbooster-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && TEMP_N="$N" TEMP_SEED="$SEED" TEMP_OUT="$TRACE" \
  node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/temp-booster.producer.test.ts \
  >"$BUILD/tempbooster-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/tempbooster-$SEED.log" >&2; exit 2; }
"$BUILD/tempboostertest" "$TRACE"
