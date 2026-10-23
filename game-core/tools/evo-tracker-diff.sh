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
cp "$ROOT/game-core/tests/evo-tracker.producer.test.ts" "$UP/test/tests/zz-dump/evo-tracker.producer.test.ts"
(cd "$UP" && TRACKER_OUT="$BUILD/evo-tracker.native.json" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/evo-tracker.producer.test.ts --maxWorkers=1)
python3 "$ROOT/game-core/tools/build-evo-tracker-proof.py" "$BUILD/evo-tracker.native.json" "$BUILD/evo-tracker.native.txt"
make -C "$ROOT/game-core" build/evo_tracker_host_test build/evo_tracker_turn_host_test
"$ROOT/game-core/build/evo_tracker_host_test" "$BUILD/evo-tracker.native.txt"
"$ROOT/game-core/build/evo_tracker_turn_host_test"
