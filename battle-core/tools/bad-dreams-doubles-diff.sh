#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/bad-dreams-doubles-test
mkdir -p "$UP/test/tests/zz-dump"
python3 "$HERE/tools/build-bad-dreams-doubles-proof.py" "$UP/test/tests/zz-dump/bad-dreams-doubles.producer.test.ts"
TRACE="$HERE/build/bad-dreams-doubles-$SEED.txt"
(cd "$UP" && DOUBLES_N=8192 DOUBLES_SEED="$SEED" DOUBLES_OUT="$TRACE" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/bad-dreams-doubles.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$HERE/build/bad-dreams-doubles-test" "$TRACE"
TURNTEST_LOCALES="$UP/locales/en" python3 "$HERE/tools/check-bad-dreams-doubles-proof.py" "$TRACE"
