#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
git -C "$UP/locales" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE/game-core" -j1 build/sync_nature_test build/sync_nature_host_test
mkdir -p "$UP/test/tests/zz-dump" "$HERE/game-core/build" "$HERE/work"
export TURNTEST_LOCALES="$UP/locales/en"
RAW="$HERE/game-core/build/sync-nature-$SEED.txt"
PREFIX="$HERE/game-core/build/sync-encounter-$SEED.txt"
python3 "$HERE/battle-core/tools/build-sync-nature-proof.py" "$UP/test/tests/zz-dump/sync-nature.producer.test.ts"
python3 "$HERE/battle-core/tools/build-sync-nature-encounter-proof.py" "$UP/test/tests/zz-dump/sync-encounter.producer.test.ts"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 SYNC_NATURE_SEED="$SEED" SYNC_NATURE_OUT="$RAW" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/sync-nature.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 SYNC_NATURE_SEED="$SEED" SYNC_ENCOUNTER_OUT="$PREFIX" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/sync-encounter.producer.test.ts --maxWorkers=1)
python3 "$HERE/battle-core/tools/check-sync-nature-proof.py" "$RAW" "$PREFIX"
CHALLENGE="$HERE/game-core/build/sync-challenge-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 SYNC_CHALLENGE=1 SYNC_NATURE_SEED="$SEED" SYNC_ENCOUNTER_OUT="$CHALLENGE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/sync-encounter.producer.test.ts --maxWorkers=1)
"$HERE/game-core/build/sync_nature_test" "$CHALLENGE"
python3 "$HERE/battle-core/tools/check-sync-challenge-context.py" "$CHALLENGE"
