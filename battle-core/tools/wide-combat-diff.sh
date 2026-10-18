#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/wide-substitute-test build/wide-combat-test
make -s -C "$HERE/../game-core" wide-combat-save-host-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-wide-combat-proof.py" "$UP/test/tests/zz-dump/wide-combat.producer.test.ts"
cp "$HERE/tests/wide-substitute.producer.test.ts" "$UP/test/tests/zz-dump/"
TURNS="$HERE/build/wide-combat-$SEED.txt"
LEAF="$HERE/build/wide-ai-substitute-$SEED.txt"
(cd "$UP" && DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/wide-combat.producer.test.ts --maxWorkers=1)
(cd "$UP" && WIDESUB_AI=1 WIDESUB_SEED="$SEED" WIDESUB_OUT="$LEAF" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/wide-substitute.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-substitute-test" "$LEAF"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-wide-combat-proof.py" "$LEAF" "$TURNS"
