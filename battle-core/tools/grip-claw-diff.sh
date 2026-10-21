#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout};N=${2:-16384};SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build};NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/grip-claw-test" "$BUILD/doublestest"
cp "$HERE/tests/grip-claw.producer.test.ts" "$UP/test/tests/zz-dump/grip-claw.producer.test.ts"
(cd "$UP" && GRIP_N="$N" GRIP_SEED="$SEED" GRIP_OUT="$BUILD/grip-$SEED.jsonl" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/grip-claw.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-grip-claw-proof.py" "$BUILD/grip-$SEED.jsonl" "$BUILD/grip-$SEED.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/grip-claw-test" "$BUILD/grip-$SEED.txt"
python3 "$HERE/tools/build-grip-claw-turn-proof.py" "$UP/test/tests/zz-dump/grip-claw-turn.producer.test.ts"
(cd "$UP" && DOUBLES_RICH=0 DOUBLES_N=2114 DOUBLES_SEED="$SEED" DOUBLES_OUT="$BUILD/grip-turns-$SEED.txt" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/grip-claw-turn.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/doublestest" "$BUILD/grip-turns-$SEED.txt"
