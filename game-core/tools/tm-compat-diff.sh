#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
UP=$(CDPATH= cd -- "$1" && pwd)
[ "$(git -C "$UP" rev-parse HEAD)" = 8555c08c823b856cbec4eb99ca84ea52a955836d ]
[ "$(git -C "$UP/locales" rev-parse HEAD)" = 270ed0a2938b2c3e2e34e7de940e84aecadf548b ]
git -C "$UP" diff --exit-code HEAD -- src >/dev/null
git -C "$UP/locales" diff --exit-code HEAD >/dev/null
node -e 'if(process.versions.node.split(".")[0]!=="22")process.exit(1)'
mkdir -p "$ROOT/work" "$UP/test/tests/zz-dump"
cp "$ROOT/game-core/tests/tm_compat.producer.test.ts" "$UP/test/tests/zz-dump/"
make -C "$ROOT/game-core" -j1 build/tm_compat_diff_test
(cd "$UP" && TM_COMPAT_OUT="$ROOT/work/tm-compat-native.txt" node node_modules/vitest/vitest.mjs run --maxWorkers=1 test/tests/zz-dump/tm_compat.producer.test.ts)
"$ROOT/game-core/build/tm_compat_diff_test" "$ROOT/work/tm-compat-native.txt"
python3 "$ROOT/game-core/tools/check-tm-compat-proof.py" "$ROOT/work/tm-compat-native.txt"
