#!/bin/sh
# Independent game caller proof; established reward producer remains unchanged.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
case "$N" in ''|*[!0-9]*) echo 'integer N required' >&2; exit 2;; esac
case "$SEED" in ''|*[!0-9]*) echo 'unsigned integer seed required' >&2; exit 2;; esac
[ "$N" -ge 120 ] && [ "$N" -le 100000 ] && [ "$SEED" -le 4294967295 ] || exit 2
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/multi_acquire_raw_test" "$BUILD/multi_acquire_diff_test" "$BUILD/multi_acquire_host_test"
cp "$HERE/../reward-core/tests/multi-held.producer.test.ts" "$UP/test/tests/zz-dump/"
cp "$HERE/tests/multi-acquire.producer.test.ts" "$UP/test/tests/zz-dump/game-multi-acquire.producer.test.ts"
RAW="$BUILD/multiheld-$SEED.txt"; TRACE="$BUILD/caller-$SEED.txt"
rm -f "$RAW" "$TRACE"
(cd "$UP" && MULTI_HELD_N="$N" MULTI_HELD_SEED="$SEED" MULTI_HELD_OUT="$RAW" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/multi-held.producer.test.ts)
"$BUILD/multi_acquire_raw_test" "$RAW"
(cd "$UP" && GC_MULTI_N="$N" GC_MULTI_SEED="$SEED" GC_MULTI_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/game-multi-acquire.producer.test.ts)
GC_LOCALES="$UP/locales/en" TURNTEST_LOCALES="$UP/locales/en" "$BUILD/multi_acquire_diff_test" "$TRACE"
"$BUILD/multi_acquire_host_test"
"$BUILD/multi_acquire_raw_test" --selftest
