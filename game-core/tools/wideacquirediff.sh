#!/bin/sh
# Independent game caller proof; established reward producer remains unchanged.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/wide_raw_test" "$BUILD/wide_acquire_diff_test" "$BUILD/wide_acquire_host_test"
cp "$HERE/../reward-core/tests/wide-held.producer.test.ts" "$UP/test/tests/zz-dump/"
cp "$HERE/tests/wide-acquire.producer.test.ts" "$UP/test/tests/zz-dump/game-wide-acquire.producer.test.ts"
RAW="$BUILD/wideheld-$SEED.txt"; TRACE="$BUILD/caller-$SEED.txt"
rm -f "$RAW" "$TRACE"
(cd "$UP" && WIDE_HELD_N="$N" WIDE_HELD_SEED="$SEED" WIDE_HELD_OUT="$RAW" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/wide-held.producer.test.ts)
"$BUILD/wide_raw_test" "$RAW"
(cd "$UP" && GC_WIDE_N="$N" GC_WIDE_SEED="$SEED" GC_WIDE_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/game-wide-acquire.producer.test.ts)
GC_LOCALES="$UP/locales/en" TURNTEST_LOCALES="$UP/locales/en" "$BUILD/wide_acquire_diff_test" "$TRACE"
"$BUILD/wide_acquire_host_test"
"$BUILD/wide_raw_test" --selftest
