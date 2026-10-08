#!/bin/sh
# Additive inventory bridge oracle using unchanged battle-core Reviver Seed fixtures.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/seed_diff_test" "$BUILD/seed_host_test"
cp "$HERE/../battle-core/tests/reviver-seed.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_SEED_OUT:-$BUILD/game-seed-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && REVIVER_N="$N" REVIVER_SEED="$SEED" REVIVER_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/reviver-seed.producer.test.ts)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/seed_diff_test" "$TRACE"
"$BUILD/seed_host_test"
