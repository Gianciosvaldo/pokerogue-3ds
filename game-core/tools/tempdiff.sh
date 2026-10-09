#!/bin/sh
# Additive canonical temporary inventory and actual Scene/BattleEnd callers.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/temp_diff_test" "$BUILD/temp_caller_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/../reward-core/tests/temp-booster.producer.test.ts" "$UP/test/tests/zz-dump/"
cp "$HERE/tests/temp-caller.producer.test.ts" "$UP/test/tests/zz-dump/"
RAW=${GC_TEMP_RAW_OUT:-$BUILD/game-temp-raw-$SEED.txt}
TRACE=${GC_TEMP_OUT:-$BUILD/game-temp-caller-$SEED.txt}
rm -f "$RAW" "$TRACE"
(cd "$UP" && TEMP_N="$N" TEMP_SEED="$SEED" TEMP_OUT="$RAW" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/temp-booster.producer.test.ts)
"$BUILD/temp_diff_test" "$RAW"
(cd "$UP" && GC_TEMP_N="$N" GC_TEMP_SEED="$SEED" GC_TEMP_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/temp-caller.producer.test.ts)
"$BUILD/temp_caller_test" "$TRACE"
"$BUILD/temp_caller_test" --selftest
