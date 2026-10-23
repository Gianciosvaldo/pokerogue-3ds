#!/bin/sh
set -eu
UP=$(CDPATH= cd -- "${1:?pinned upstream checkout}" && pwd)
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
git -C "$UP/locales" diff --exit-code HEAD >/dev/null
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/stat-loot-test"
cp "$HERE/tests/stat-loot.producer.test.ts" "$UP/test/tests/zz-dump/stat-loot.producer.test.ts"
(cd "$UP" && STAT_LOOT_OUT="$BUILD/stat-loot.native.json" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/stat-loot.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-stat-loot-proof.py" "$BUILD/stat-loot.native.json" "$BUILD/stat-loot.native.txt" "$BUILD/stat-loot-save.native.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/stat-loot-test" "$BUILD/stat-loot.native.txt"
