#!/bin/sh
# Actual pinned Classic terminal phase; gameplay-driven whole-run clears excluded.
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
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/classic_win_diff_test" "$BUILD/classic_win_host_test"
cp "$HERE/tests/classic-win.producer.test.ts" "$UP/test/tests/zz-dump/game-classic-win.producer.test.ts"
TRACE="$BUILD/classic-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && GC_CLASSIC_N="$N" GC_CLASSIC_SEED="$SEED" GC_CLASSIC_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/game-classic-win.producer.test.ts)
GC_LOCALES="$UP/locales/en" TURNTEST_LOCALES="$UP/locales/en" "$BUILD/classic_win_diff_test" "$TRACE"
"$BUILD/classic_win_host_test"
