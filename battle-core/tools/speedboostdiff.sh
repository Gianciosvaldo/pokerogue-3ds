#!/bin/sh
# Additive bounded Speed Boost turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/speedboosttest"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/speed-boost.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/speed-boost-$SEED.txt"
(cd "$UP" && SPEEDBOOST_N="$N" SPEEDBOOST_SEED="$SEED" SPEEDBOOST_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/speed-boost.producer.test.ts --maxWorkers=1) > "$BUILD/speed-boost-$SEED-producer.log" 2>&1
sed -n '/SPEEDBOOST cases=/p' "$BUILD/speed-boost-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/speedboosttest" "$TRACE"
