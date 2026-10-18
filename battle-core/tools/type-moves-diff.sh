#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/type-moves-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/type-moves.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-type-moves-turn-proof.py" "$UP/test/tests/zz-dump/type-moves-turns.producer.test.ts"
TRACE="$HERE/build/type-moves-$SEED.txt"
TURNS="$HERE/build/type-move-turns-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 TYPEMOVES_SEED="$SEED" TYPEMOVES_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/type-moves.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 DOUBLES_N=8448 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/type-moves-turns.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/type-moves-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-type-moves-proof.py" "$TRACE" "$TURNS"
