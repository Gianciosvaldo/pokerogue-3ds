#!/bin/sh
# Bounded lure modifier add/lapse/chance oracle, Node22 only.
set -eu
UP=${1:?upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-lure-build}
case "$(node --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2 ;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/lure.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/lure-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && LURE_N="$N" LURE_SEED="$SEED" LURE_OUT="$TRACE" \
  node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/lure.producer.test.ts \
  >"$BUILD/lure-$SEED.log" 2>&1) || { echo "producer failed: $BUILD/lure-$SEED.log" >&2; exit 2; }
"$BUILD/luretest" "$TRACE"
