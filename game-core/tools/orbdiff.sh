#!/bin/sh
# Additive inventory bridge oracle using unchanged battle-core Orb fixtures.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/orb_diff_test" "$BUILD/orb_host_test"
cp "$HERE/../battle-core/tests/status-orb.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_ORB_OUT:-$BUILD/game-orb-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && STATUSORB_N="$N" STATUSORB_SEED="$SEED" STATUSORB_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/status-orb.producer.test.ts)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/orb_diff_test" "$TRACE"
"$BUILD/orb_host_test"
