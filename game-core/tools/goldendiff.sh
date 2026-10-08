#!/bin/sh
# Additive inventory bridge oracle using unchanged battle-core Golden Punch fixtures.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/golden_diff_test" "$BUILD/golden_host_test"
cp "$HERE/../battle-core/tests/golden-punch.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_GOLDEN_OUT:-$BUILD/game-golden-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GOLDEN_N="$N" GOLDEN_SEED="$SEED" GOLDEN_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/golden-punch.producer.test.ts)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/golden_diff_test" "$TRACE"
"$BUILD/golden_host_test"
