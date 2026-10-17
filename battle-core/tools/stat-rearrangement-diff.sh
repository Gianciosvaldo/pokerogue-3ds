#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/stat-rearrangement-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/stat-rearrangement.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-stat-rearrangement-turn-proof.py" "$UP/test/tests/zz-dump/stat-rearrangement-turns.producer.test.ts"
LEAF="$HERE/build/stat-rearrangement-$SEED.txt"
TURNS="$HERE/build/stat-rearrangement-turns-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 STATREARRANGE_SEED="$SEED" STATREARRANGE_OUT="$LEAF" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/stat-rearrangement.producer.test.ts --maxWorkers=1)
(cd "$UP" && DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/stat-rearrangement-turns.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/stat-rearrangement-test" "$LEAF"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-stat-rearrangement-proof.py" "$LEAF" "$TURNS"
