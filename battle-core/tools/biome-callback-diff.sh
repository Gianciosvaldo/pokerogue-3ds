#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}; SEED=${2:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
git -C "$UP" diff --quiet
git -C "$UP/locales" diff --quiet
test "$(node -p 'process.versions.node.split(".")[0]')" = 22
make -s -C "$HERE" build/biome-callback-test
mkdir -p "$UP/test/tests/zz-dump"
for KIND in callback prefix reachability; do
 python3 "$HERE/tools/build-biome-$KIND-proof.py" "$UP/test/tests/zz-dump/biome-$KIND.producer.test.ts"
done
RAW="$HERE/build/biome-callback-$SEED.txt"; PREFIX="$HERE/build/biome-prefix-$SEED.txt"
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 BIOME_CALLBACK_SEED="$SEED" BIOME_CALLBACK_OUT="$RAW" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/biome-callback.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 BIOME_CALLBACK_SEED="$SEED" BIOME_CALLBACK_OUT="$PREFIX" node node_modules/vitest/vitest.mjs run test/tests/zz-dump/biome-prefix.producer.test.ts --maxWorkers=1)
(cd "$UP" && NODE_OPTIONS=--max-old-space-size=4096 node node_modules/vitest/vitest.mjs run test/tests/zz-dump/biome-reachability.producer.test.ts --maxWorkers=1)
export TURNTEST_LOCALES="$UP/locales/en"
python3 "$HERE/tools/check-biome-callback-proof.py" "$RAW" "$PREFIX"
