#!/bin/sh
# Re-dump game/gen/gm_item_ui.c (icon, name and description of every reward item) from an upstream checkout prepared by
# battle-core/tools/setup-upstream.sh. Usage: game/tools/redump-item-ui.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/tools/dump-item-ui.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DUMP_OUT="$HERE/gen/gm_item_ui.c" npx vitest run test/tests/zz-dump/dump-item-ui.test.ts >/dev/null)
echo "game/gen/gm_item_ui.c"
