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
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/fused-held-test" "$BUILD/statitemtest" "$BUILD/crititemtest"
python3 "$HERE/tools/build-fused-held-proof.py" "$UP/test/tests/zz-dump/fused-held.producer.test.ts"
for seed in 777 2034; do
    trace="$BUILD/fused-held-$seed.txt"
    (cd "$UP" && FUSEDHELD_SEED="$seed" FUSEDHELD_OUT="$trace" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/fused-held.producer.test.ts --maxWorkers=1)
    TURNTEST_LOCALES="$UP/locales/en" "$BUILD/fused-held-test" "$trace"
done
cp "$HERE/tests/effective-stat-item.producer.test.ts" "$UP/test/tests/zz-dump/effective-stat-item.producer.test.ts"
cp "$HERE/tests/critical-item.producer.test.ts" "$UP/test/tests/zz-dump/critical-item.producer.test.ts"
(cd "$UP" && STATITEM_N=2000 STATITEM_SEED=777 STATITEM_OUT="$BUILD/statitem-retained-777.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/effective-stat-item.producer.test.ts --maxWorkers=1)
(cd "$UP" && CRITITEM_N=2000 CRITITEM_SEED=777 CRITITEM_OUT="$BUILD/crititem-retained-777.txt" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/critical-item.producer.test.ts --maxWorkers=1)
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/statitemtest" "$BUILD/statitem-retained-777.txt"
TURNTEST_LOCALES="$UP/locales/en" "$BUILD/crititemtest" "$BUILD/crititem-retained-777.txt"
