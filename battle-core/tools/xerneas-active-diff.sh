#!/bin/sh
set -eu
UP=${1:?pinned upstream}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/xerneas-active-test
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tests/xerneas-active.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$HERE/build/xerneas-active-$SEED.txt"
(cd "$UP" && XERNEAS_N=8192 XERNEAS_SEED="$SEED" XERNEAS_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/xerneas-active.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/xerneas-active-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-xerneas-active-proof.py" "$TRACE"
