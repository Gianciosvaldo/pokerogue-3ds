#!/bin/sh
# Additive bounded Critical item turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/critical-item.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/critical-item-$SEED.txt"
(cd "$UP" && CRITITEM_N="$N" CRITITEM_SEED="$SEED" CRITITEM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/critical-item.producer.test.ts --maxWorkers=1) > "$BUILD/critical-item-$SEED-producer.log" 2>&1
sed -n '/CRITITEM cases=/p' "$BUILD/critical-item-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/crititemtest" "$TRACE"
