#!/bin/sh
# Additive bounded Soul Dew turn suite; Node 22 and pinned external upstream.
set -eu
UP=${1:?upstream checkout}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-"$HERE/build"}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required" >&2; exit 2 ;; esac
make -s -C "$HERE" -j2 BUILD="$BUILD"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/soul-dew.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/soul-dew-$SEED.txt"
(cd "$UP" && SOUL_N="$N" SOUL_SEED="$SEED" SOUL_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/soul-dew.producer.test.ts --maxWorkers=1) > "$BUILD/soul-dew-$SEED-producer.log" 2>&1
sed -n '/SOUL cases=/p' "$BUILD/soul-dew-$SEED-producer.log"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/souldewtest" "$TRACE"
