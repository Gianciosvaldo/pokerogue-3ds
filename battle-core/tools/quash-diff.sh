#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/quash-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/quash.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-quash-turn-proof.py" "$UP/test/tests/zz-dump/quash-turns.producer.test.ts"
TRACE="$HERE/build/quash.txt"
TURNS="$HERE/build/quash-turns-777.txt"
(cd "$UP" && QUASH_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/quash.producer.test.ts --maxWorkers=1)
(cd "$UP" && DOUBLES_N=8192 DOUBLES_SEED=777 DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/quash-turns.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/quash-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-quash-proof.py" "$TRACE" "$TURNS"
