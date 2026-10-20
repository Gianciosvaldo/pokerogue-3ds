#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/tera-phase-test build/tera-cleanup-test build/wide-combat-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/tera-phase.producer.test.ts" "$UP/test/tests/zz-dump/"
python3 "$HERE/tools/build-tera-phase-turn-proof.py" "$UP/test/tests/zz-dump/tera-phase-turns.producer.test.ts"
TRACE="$HERE/build/tera-phase-$SEED.txt"; TURNS="$HERE/build/tera-phase-turns-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 TERA_SEED="$SEED" TERA_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/tera-phase.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=6144 DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TURNS" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/tera-phase-turns.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/tera-phase-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-complete-turns.py" "$HERE/build/wide-combat-test" "$TURNS"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-tera-phase-proof.py" "$TRACE" "$TURNS"
python3 "$HERE/tools/build-tera-cleanup-proof.py" "$UP/test/tests/zz-dump/tera-cleanup.producer.test.ts"
CLEANUP="$HERE/build/tera-cleanup-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 RESETARENA_N=2048 RESETARENA_SEED="$SEED" RESETARENA_OUT="$HERE/build/tera-cleanup-full-$SEED.txt" TERA_CLEANUP_OUT="$CLEANUP" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/tera-cleanup.producer.test.ts --maxWorkers=1)
"$HERE/build/tera-cleanup-test" "$CLEANUP"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-tera-cleanup-proof.py" "$CLEANUP"
