#!/bin/sh
# Zero to Hero: boundary matrix and switch-out turns from the pinned source, replayed by the port.
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/zero-to-hero-test build/turntest
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/zero-to-hero.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-zero-to-hero-turn-source.py" "$HERE/tests/turntest.producer.test.ts" "$UP/test/tests/zz-dump/zero-to-hero-turn.producer.test.ts"
API="$HERE/build/zero-to-hero-$SEED.txt"
TURNS="$HERE/build/zero-to-hero-turns-$SEED.txt"
(cd "$UP" && ZEROTOHERO_N=2016 ZEROTOHERO_SEED="$SEED" ZEROTOHERO_OUT="$API" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/zero-to-hero.producer.test.ts --maxWorkers=1)
(cd "$UP" && TURN_N=2000 TURN_SEED="$SEED" TURN_TRAINER=1 TURN_SUMMON=1 TURN_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/zero-to-hero-turn.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/zero-to-hero-test" "$API"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-zero-to-hero-turns.py" "$TURNS"
