#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; N=${2:-16384}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/berry-moves-test build/doublestest
make -s -C "$HERE/../game-core" berry-moves-host-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/berry-moves.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/berry-moves-$SEED.txt"
(cd "$UP" && BERRYMOVES_N="$N" BERRYMOVES_SEED="$SEED" BERRYMOVES_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/berry-moves.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/berry-moves-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-berry-moves-proof.py" "$TRACE"
python3 "$HERE/tools/build-berry-move-turn-proof.py" "$UP/test/tests/zz-dump/berry-move-turns.producer.test.ts"
TURNS="$HERE/build/berry-move-turns-$SEED.txt"
(cd "$UP" && DOUBLES_N=2050 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/berry-move-turns.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/doublestest" "$TURNS"
