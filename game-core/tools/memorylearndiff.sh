#!/bin/sh
# Memory Mushroom and TM learning (gc_phases.c learned_tm, gc_reward.c, the shop choice) against the pinned upstream: the real
# LearnMovePhase.learnMove and SelectModifierPhase.applyModifier, written by tests/memory-learn.producer.test.ts and replayed by
# tests/memory_learn_diff_test.c. The fixture tests/fixtures/memory-learn.native.txt is that trace; `make test` replays it without upstream.
# usage: tools/memorylearndiff.sh <upstream checkout>
set -eu
UP=${1:?pinned upstream dir}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/memory_learn_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/memory-learn.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${ML_OUT:-$BUILD/memory-learn.txt}
rm -f "$TRACE"
(cd "$UP" && ML_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/memory-learn.producer.test.ts)
"$BUILD/memory_learn_diff_test" "$TRACE"
