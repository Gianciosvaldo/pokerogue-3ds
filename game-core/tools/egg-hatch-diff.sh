#!/bin/sh
set -eu
UP=${1:?pinned upstream checkout}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-$HERE/build};NODE=${NODE:-node}
test "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d
test "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b
test "$("$NODE" -p 'process.versions.node.split(".")[0]')" = 22
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
cp "$HERE/tests/egg-hatch.producer.test.ts" "$UP/test/tests/zz-dump/egg-hatch.producer.test.ts"
(cd "$UP" && EGG_HATCH_OUT="$BUILD/egg-hatch.json" "$NODE" node_modules/vitest/vitest.mjs run test/tests/zz-dump/egg-hatch.producer.test.ts --maxWorkers=1)
python3 "$HERE/tools/build-egg-hatch-proof.py" "$BUILD/egg-hatch.json" "$BUILD/egg-hatch.txt"
make -s -C "$HERE" BUILD="$BUILD" "$BUILD/egg_hatch_diff_test"
"$BUILD/egg_hatch_diff_test" "$BUILD/egg-hatch.txt"
