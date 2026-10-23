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
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/pickup-test"
cp "$HERE/tests/pickup.producer.test.ts" "$UP/test/tests/zz-dump/pickup.producer.test.ts"
(cd "$UP" && PICKUP_OUT="$BUILD/pickup.native.json" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/pickup.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-pickup-proof.py" "$BUILD/pickup.native.json" "$BUILD/pickup.native.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/pickup-test" "$BUILD/pickup.native.txt"
