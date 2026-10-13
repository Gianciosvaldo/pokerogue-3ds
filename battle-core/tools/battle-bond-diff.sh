#!/bin/sh
set -eu
UP=${1:?pinned upstream}; N=${2:-2000}; SEED=${3:-777}; KIND=${4:-both}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ] || exit 2
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ] || exit 2
git -C "$UP" diff --quiet || exit 2
case "$(node --version)" in v22.*) ;; *) exit 2 ;; esac
case "$KIND" in both|phase|turn) ;; *) exit 2 ;; esac
make -s -C "$HERE" -j2 build/battle-bond-test build/battle-bond-turn-test
mkdir -p "$UP/test/tests/zz-dump"
if [ "$KIND" != turn ]; then
 cp "$HERE/tests/battle-bond.producer.test.ts" "$UP/test/tests/zz-dump/"
 TRACE="$HERE/build/battle-bond-phase-$SEED.txt"
 (cd "$UP" && BATTLEBOND_N="$N" BATTLEBOND_SEED="$SEED" BATTLEBOND_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/battle-bond.producer.test.ts --maxWorkers=1) > "$HERE/build/battle-bond-phase-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/battle-bond-test" "$TRACE"
fi
if [ "$KIND" != phase ]; then
 python3 "$HERE/tools/build-battle-bond-turn-source.py" "$UP/test/tests/zz-dump/battle-bond-turn.producer.test.ts"
 TRACE="$HERE/build/battle-bond-turn-$SEED.txt"
 (cd "$UP" && BATTLE_BOND_TURN_N="$N" BATTLE_BOND_TURN_SEED="$SEED" BATTLE_BOND_TURN_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/battle-bond-turn.producer.test.ts --maxWorkers=1) > "$HERE/build/battle-bond-turn-source-$SEED.log" 2>&1
 TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/battle-bond-turn-test" "$TRACE"
fi
