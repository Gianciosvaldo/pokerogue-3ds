#!/bin/sh
# Challenge Mode's starter rules (Fresh Start: STARTER_COST, STARTER_CHOICE, STARTER_SELECT_MODIFY, STARTER_MODIFY) against the pinned upstream,
# one seed per call: the producer runs the real getRunValueLimit, getSpeciesStarterValue, checkStarterValidForChallenge, getStarterData and
# SelectStarterPhase.initBattle in a Challenge Mode game mode; the comparator replays the trace through game-core.
# usage: tools/challengestartersdiff.sh <upstream checkout> [N=400] [seed=777]
set -eu
UP=${1:?pinned upstream dir}; N=${2:-400}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/challenge_starters_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/challenge-starters.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_CHALLENGE_OUT:-$BUILD/challenge-starters-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_CHALLENGE_N="$N" GC_CHALLENGE_SEED="$SEED" GC_CHALLENGE_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/challenge-starters.producer.test.ts)
"$BUILD/challenge_starters_diff_test" "$TRACE"
