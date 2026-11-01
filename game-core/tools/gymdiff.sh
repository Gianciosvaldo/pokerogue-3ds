#!/bin/sh
# Gym leaders, the Elite Four and the Champion in the run (src/gc_encounter.c) against the pinned upstream: the cases are the ones run-core's gym_e4
# producer writes (the real gym leader of a boss wave, the real fixed Elite Four / Champion of waves 182..190, their templates, levels and every
# member's constructor state), replayed through gc_new_battle as the app starts the wave and compared by tests/gym_e4_diff_test.c.
# usage: tools/gymdiff.sh <upstream checkout> [N=3000] [seed=777]   (~1 s per 40 cases)
set -eu
UP=${1:?pinned upstream dir}; N=${2:-3000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/gym_e4_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$ROOT/run-core/tests/gym_e4.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_GYM_OUT:-$BUILD/gym-e4-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && DIFF_N="$N" DIFF_SEED="$SEED" DIFF_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/gym_e4.producer.test.ts >/dev/null)
"$BUILD/gym_e4_diff_test" "$TRACE"
