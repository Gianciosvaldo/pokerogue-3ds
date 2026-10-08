#!/bin/sh
# Separate game-core adapter oracle; established reward checks are unchanged.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/money_diff_test" "$BUILD/money_host_test"
cp "$HERE/tests/money.producer.test.ts" "$UP/test/tests/zz-dump/game-money.producer.test.ts"
TRACE=${GC_MONEY_OUT:-$BUILD/game-money-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_MONEY_N="$N" GC_MONEY_SEED="$SEED" GC_MONEY_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/game-money.producer.test.ts)
"$BUILD/money_diff_test" "$TRACE"
"$BUILD/money_host_test"
