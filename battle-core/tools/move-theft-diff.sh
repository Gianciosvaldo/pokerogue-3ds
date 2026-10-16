#!/bin/sh
set -eu
UP=${1:?upstream checkout}; N=${2:-16384}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}; NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/move-theft-test"
make -s -C "$HERE/../game-core" build/move_theft_tier_diff_test move-theft-host-test
cp "$HERE/tests/move-theft.producer.test.ts" "$UP/test/tests/zz-dump/move-theft.producer.test.ts"
cp "$HERE/tests/move-theft-tiers.producer.test.ts" "$UP/test/tests/zz-dump/move-theft-tiers.producer.test.ts"
(cd "$UP" && THEFT_N="$N" THEFT_SEED="$SEED" THEFT_OUT="$BUILD/move-theft-$SEED.jsonl" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/move-theft.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-move-theft-proof.py" "$BUILD/move-theft-$SEED.jsonl" "$BUILD/move-theft-$SEED.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/move-theft-test" "$BUILD/move-theft-$SEED.txt"
(cd "$UP" && THEFT_TIERS_OUT="$BUILD/move-theft-tiers.txt" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/move-theft-tiers.producer.test.ts --maxWorkers=1)
"$HERE/../game-core/build/move_theft_tier_diff_test" "$BUILD/move-theft-tiers.txt"
