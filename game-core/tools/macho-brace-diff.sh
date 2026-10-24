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
for name in macho-brace macho-capture; do
    cp "$ROOT/game-core/tests/$name.producer.test.ts" "$UP/test/tests/zz-dump/$name.producer.test.ts"
done
(cd "$UP" && MACHO_OUT="$BUILD/macho-brace.native.json" MACHO_CAPTURE_OUT="$BUILD/macho-capture.native.json" \
    node node_modules/vitest/vitest.mjs run test/tests/zz-dump/macho-brace.producer.test.ts \
    test/tests/zz-dump/macho-capture.producer.test.ts --maxWorkers=1)
python3 "$ROOT/game-core/tools/build-macho-brace-proof.py" "$BUILD/macho-brace.native.json" \
    "$BUILD/macho-capture.native.json" "$BUILD/macho-brace.native.txt"
make -C "$ROOT/game-core" build/macho_brace_host_test
"$ROOT/game-core/build/macho_brace_host_test" "$BUILD/macho-brace.native.txt"
