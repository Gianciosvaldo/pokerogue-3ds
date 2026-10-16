#!/bin/sh
# Actual pinned addExp/addModifier; pre-execution dynamic LevelUp requests.
set -eu
UP=${1:?upstream checkout}; N=${2:-12288}; SEED=${3:-777}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build}; NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/wide_progression_diff_test" "$BUILD/wide_progression_host_test"
cp "$HERE/tests/wide-progression.producer.test.ts" "$UP/test/tests/zz-dump/wide-progression.producer.test.ts"
(cd "$UP" && PROGRESSION_N="$N" PROGRESSION_SEED="$SEED" PROGRESSION_OUT="$BUILD/wide-progression-$SEED.txt" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/wide-progression.producer.test.ts --maxWorkers=1)
"$BUILD/wide_progression_host_test"
"$BUILD/wide_progression_diff_test" "$BUILD/wide-progression-$SEED.txt"
