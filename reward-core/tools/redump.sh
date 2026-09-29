#!/bin/sh
# Re-dump reward-core's generated data (gen/out/reward_data.c, reward_items.h) from an upstream checkout
# prepared by battle-core/tools/setup-upstream.sh. Usage: tools/redump.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/gen/dump-rewards.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DUMP_OUT="$HERE/gen/out/reward_data.c" npx vitest run test/tests/zz-dump/dump-rewards.test.ts >/dev/null)
echo "gen/out/reward_data.c gen/out/reward_items.h"
