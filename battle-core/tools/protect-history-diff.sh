#!/bin/sh
set -eu
UP=$(CDPATH= cd -- "${1:?pinned upstream checkout}" && pwd)
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD=${BUILD:-$ROOT/battle-core/build}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
git -C "$UP/locales" diff --exit-code HEAD >/dev/null
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$ROOT/battle-core/tests/protect-history.producer.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && PROTECT_OUT="$BUILD/protect.native.json" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/protect-history.producer.test.ts --maxWorkers=1)
python3 "$ROOT/battle-core/tools/build-protect-history-proof.py" "$BUILD/protect.native.json" "$BUILD/protect.native.txt"
gzip -cd "$ROOT/game-core/tests/fixtures/protect-history.native.txt.gz" > "$BUILD/protect.retained.txt"
cmp "$BUILD/protect.native.txt" "$BUILD/protect.retained.txt"
make -C "$ROOT/battle-core" build/protect-history-test build/protect-history-guards-test
make -C "$ROOT/game-core" build/protect_history_host_test
python3 "$ROOT/battle-core/tools/check-protect-history-proof.py" --run-only
