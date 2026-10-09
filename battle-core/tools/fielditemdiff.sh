#!/bin/sh
# Additive bounded Field effect item turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/field-effect.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/field-effect-$SEED.txt"
(cd "$UP" && FIELDITEM_N="$N" FIELDITEM_SEED="$SEED" FIELDITEM_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/field-effect.producer.test.ts --maxWorkers=1) > "$BUILD/field-effect-$SEED-producer.log" 2>&1
sed -n '/FIELDITEM cases=/p' "$BUILD/field-effect-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/fielditemtest" "$TRACE"
