#!/bin/sh
# Fleeing (AttemptRunPhase: the escape chance, the roll, the success and failure branches) against the pinned upstream,
# one seed per call. usage: tools/fleediff.sh <upstream checkout> [N=600] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-600}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/flee_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/flee.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${FLEE_OUT:-$BUILD/game-flee-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && FLEE_N="$N" FLEE_SEED="$SEED" FLEE_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/flee.producer.test.ts)
"$BUILD/flee_diff_test" "$TRACE"
