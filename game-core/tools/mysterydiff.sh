#!/bin/sh
# Mystery encounter actions (the real encounters' own option code, card 82's harness) against the pinned upstream,
# one seed per call. Families so far: Department Store Sale, Shady Vitamin Dealer, Lost at Sea, Field Trip, Part Timer, An Offer You Can't Refuse, The Pokemon Salesman, Fun and Games (plays the real encounter's phases, GC_FG_N cases, default 400).
# usage: tools/mysterydiff.sh <upstream checkout> [N=2000] [seed=777]
# GC_MYS_ONLY=fungames runs that family alone (GC_FG_N cases, default 400; ~0.7 s each).
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}; BUILD=${BUILD:-$HERE/build}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
if [ "${GC_MYS_ONLY:-}" = fungames ]; then
  make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_fungames_diff_test"
  mkdir -p "$UP/test/tests/zz-dump"
  cp "$HERE/tests/mystery-fungames.producer.test.ts" "$UP/test/tests/zz-dump/"
  TRACE=${GC_MYS_OUT:-$BUILD/game-mystery-$SEED.txt}
  rm -f "${TRACE%.txt}-fungames.txt"
  (cd "$UP" && GC_FG_N="${GC_FG_N:-400}" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-fungames.producer.test.ts)
  exec "$BUILD/mystery_fungames_diff_test" "${TRACE%.txt}-fungames.txt"
fi
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/mystery_actions_diff_test" "$BUILD/mystery_shady_diff_test" "$BUILD/mystery_lost_diff_test" "$BUILD/mystery_trip_diff_test" "$BUILD/mystery_timer_diff_test" "$BUILD/mystery_offer_diff_test" "$BUILD/mystery_salesman_diff_test" "$BUILD/mystery_fungames_diff_test"
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/mystery-actions.producer.test.ts" "$HERE/tests/mystery-salesman.producer.test.ts" "$HERE/tests/mystery-fungames.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE=${GC_MYS_OUT:-$BUILD/game-mystery-$SEED.txt}
SHADY=${TRACE%.txt}-shady.txt
LOST=${TRACE%.txt}-lost.txt
TRIP=${TRACE%.txt}-trip.txt
TIMER=${TRACE%.txt}-timer.txt
OFFER=${TRACE%.txt}-offer.txt
SALES=${TRACE%.txt}-salesman.txt
FUN=${TRACE%.txt}-fungames.txt
rm -f "$TRACE" "$SHADY" "$LOST" "$TRIP" "$TIMER" "$OFFER" "$SALES" "$FUN"
(cd "$UP" && GC_MYS_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-actions.producer.test.ts)
(cd "$UP" && GC_MYS_N="$N" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-salesman.producer.test.ts)
(cd "$UP" && GC_FG_N="${GC_FG_N:-400}" GC_MYS_SEED="$SEED" GC_MYS_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/mystery-fungames.producer.test.ts)
"$BUILD/mystery_actions_diff_test" "$TRACE"
"$BUILD/mystery_shady_diff_test" "$SHADY"
"$BUILD/mystery_lost_diff_test" "$LOST"
"$BUILD/mystery_trip_diff_test" "$TRIP"
"$BUILD/mystery_timer_diff_test" "$TIMER"
"$BUILD/mystery_offer_diff_test" "$OFFER"
"$BUILD/mystery_salesman_diff_test" "$SALES"
"$BUILD/mystery_fungames_diff_test" "$FUN"
