#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}; NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/scene-transfer-test"
cp "$HERE/tests/scene-transfer.producer.test.ts" "$UP/test/tests/zz-dump/scene-transfer.producer.test.ts"
(cd "$UP" && SCENE_TRANSFER_OUT="$BUILD/scene-transfer.json" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/scene-transfer.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-scene-transfer-proof.py" "$BUILD/scene-transfer.json" "$BUILD/scene-transfer.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/scene-transfer-test" "$BUILD/scene-transfer.txt"
