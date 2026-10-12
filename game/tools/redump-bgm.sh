#!/bin/sh
# Re-dump the music tables (game/gen/gm_bgm_data.c) and the upstream answers the host test compares with
# (game/tests/bgm_expected.c) from an upstream checkout prepared by battle-core/tools/setup-upstream.sh.
# Usage: tools/redump-bgm.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tools/dump-bgm.test.ts" "$UP/test/tests/zz-dump/gm-dump-bgm.test.ts"
(cd "$UP" && DUMP_OUT_DATA="$HERE/gen/gm_bgm_data.c" DUMP_OUT_EXPECTED="$HERE/tests/bgm_expected.c" \
  npx vitest run test/tests/zz-dump/gm-dump-bgm.test.ts)
echo "gen/gm_bgm_data.c tests/bgm_expected.c"
