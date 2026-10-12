#!/bin/sh
# TrainerVictoryPhase: replays upstream's own phase for every trainerConfigs entry, with its voucher unlocked or not and
# every ModifierRewardPhase it queued run (the boss voucher, the rivals' reward items), through game-core's trainer victory.
# Usage: tools/trainervictorydiff.sh <upstream checkout> [N] [seed]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares (pinned commit); needs Node 22.
set -eu
UP=${1:?pinned upstream dir}; N=${2:-2000}; SEED=${3:-777}
case "$N" in ''|*[!0-9]*) echo 'integer N required' >&2; exit 2;; esac
case "$SEED" in ''|*[!0-9]*) echo 'unsigned integer seed required' >&2; exit 2;; esac
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
NODE=${NODE:-node}
case "$("$NODE" --version)" in v22.*) ;; *) echo 'Node 22 required' >&2; exit 2;; esac
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || { echo 'Wrong upstream pin' >&2; exit 2; }
BUILD=${BUILD:-$HERE/build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -j2 -C "$HERE" BUILD="$BUILD" "$BUILD/trainer_victory_diff_test"
cp "$HERE/tests/trainer-victory.producer.test.ts" "$UP/test/tests/zz-dump/game-trainer-victory.producer.test.ts"
TRACE=${GC_VOUCHER_OUT:-$BUILD/game-trainer-victory-$SEED.txt}
rm -f "$TRACE"
(cd "$UP" && GC_VOUCHER_N="$N" GC_VOUCHER_SEED="$SEED" GC_VOUCHER_OUT="$TRACE" "$NODE" node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/game-trainer-victory.producer.test.ts >"$BUILD/game-trainer-victory-$SEED.log" 2>&1) \
  || { echo "producer failed (log: $BUILD/game-trainer-victory-$SEED.log)" >&2; exit 2; }
GC_LOCALES="$UP/locales/en" "$BUILD/trainer_victory_diff_test" "$TRACE"
