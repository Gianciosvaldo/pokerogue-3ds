#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/costar-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-costar-proof.py" "$UP/test/tests/zz-dump/costar.producer.test.ts"
python3 "$HERE/tools/build-costar-turn-proof.py" "$UP/test/tests/zz-dump/costar-turns.producer.test.ts"
RAW="$HERE/build/costar-$SEED.txt"; TURNS="$HERE/build/costar-turns-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 COSTAR_SEED="$SEED" COSTAR_OUT="$RAW" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/costar.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/costar-turns.producer.test.ts --maxWorkers=1)
export TURNTEST_LOCALES="$UP/locales/en"
python3 "$HERE/tools/check-complete-turns.py" "$HERE/build/wide-combat-test" "$TURNS"
python3 "$HERE/tools/check-complete-turns-guard.py" "$TURNS"
python3 "$HERE/tools/check-costar-proof.py" "$RAW"
