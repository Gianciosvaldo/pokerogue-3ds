#!/bin/sh
set -eu
UP=$(CDPATH= cd -- "${1:?pinned upstream checkout}" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/game-core/build}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$ROOT/game-core/tests/held-lock.producer.test.ts" "$UP/test/tests/zz-dump/held-lock.producer.test.ts"
(cd "$UP" && LOCK_OUT="$BUILD/held-lock.native.json" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/held-lock.producer.test.ts --maxWorkers=1)
python3 "$ROOT/game-core/tools/build-held-lock-proof.py" "$BUILD/held-lock.native.json" "$BUILD/held-lock.native.txt"
make -C "$ROOT/game-core" build/held_lock_host_test
"$ROOT/game-core/build/held_lock_host_test" "$BUILD/held-lock.native.txt"
