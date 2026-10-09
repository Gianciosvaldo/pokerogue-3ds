#!/bin/sh
# Additive bounded Effective stat item turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/effective-stat-item.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/effective-stat-item-$SEED.txt"
(cd "$UP" && STATITEM_N="$N" STATITEM_SEED="$SEED" STATITEM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/effective-stat-item.producer.test.ts --maxWorkers=1) > "$BUILD/effective-stat-item-$SEED-producer.log" 2>&1
sed -n '/STATITEM cases=/p' "$BUILD/effective-stat-item-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/statitemtest" "$TRACE"
