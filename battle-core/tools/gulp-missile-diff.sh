#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/gulp-missile-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/gulp-missile.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-gulp-missile-turn-proof.py" "$UP/test/tests/zz-dump/gulp-missile-turns.producer.test.ts"
TRACE="$HERE/build/gulp-missile-$SEED.txt"; TURNS="$HERE/build/gulp-missile-turns-$SEED.txt"
CONT="$HERE/build/gulp-missile-continuation-$SEED.txt"
python3 "$HERE/tools/build-gulp-missile-continuation-proof.py" "$UP/test/tests/zz-dump/gulp-missile-continuation.producer.test.ts"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 GULP_SEED="$SEED" GULP_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/gulp-missile.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/gulp-missile-turns.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$CONT" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/gulp-missile-continuation.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$CONT"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/gulp-missile-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-gulp-missile-proof.py" "$TRACE"
