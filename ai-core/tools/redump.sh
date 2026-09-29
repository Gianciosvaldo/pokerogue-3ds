#!/bin/sh
# Re-dump ai-core's generated data (gen/out/ai_data.c) from an upstream checkout prepared by
# battle-core/tools/setup-upstream.sh. Usage: tools/redump.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
cp "$HERE/gen/dump-ai.test.ts" "$UP/test/tests/zz-dump/"
(cd "$UP" && DUMP_OUT="$HERE/gen/out/ai_data.c" npx vitest run test/tests/zz-dump/dump-ai.test.ts >/dev/null)
echo "gen/out/ai_data.c"
grep -o 'ai_unknown_notes = "[^"]*"' "$HERE/gen/out/ai_data.c"
