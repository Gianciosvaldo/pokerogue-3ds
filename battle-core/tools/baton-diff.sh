#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout};SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build};NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/baton-switch-test" "$BUILD/doublestest"
cp "$HERE/tests/baton-switch.producer.test.ts" "$UP/test/tests/zz-dump/baton-switch.producer.test.ts"
(cd "$UP" && BATON_SWITCH_OUT="$BUILD/baton-switch.json" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/baton-switch.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-baton-switch-proof.py" "$BUILD/baton-switch.json" "$BUILD/baton-switch.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/baton-switch-test" "$BUILD/baton-switch.txt"
python3 "$HERE/tools/build-baton-turn-proof.py" "$UP/test/tests/zz-dump/baton-turn.producer.test.ts"
(cd "$UP" && DOUBLES_RICH=0 DOUBLES_N=2050 DOUBLES_SEED="$SEED" DOUBLES_OUT="$BUILD/baton-turns-$SEED.txt" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/baton-turn.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/doublestest" "$BUILD/baton-turns-$SEED.txt"
