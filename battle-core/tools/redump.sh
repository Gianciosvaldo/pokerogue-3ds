#!/bin/sh
# Re-dump upstream data and regenerate the C tables; then report closure drift.
# Usage: tools/redump.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cp "$HERE/gen/dump-upstream.test.ts" "$UP/test/tests/zz-dump/"
cp "$HERE/gen/out/closures.txt" /tmp/closures.before.txt 2>/dev/null || true
(cd "$UP" && DUMP_OUT="$HERE/gen/upstream" npx vitest run test/tests/zz-dump/dump-upstream.test.ts >/dev/null)
python3 "$HERE/gen/gen_tables.py"
python3 "$HERE/gen/check_closures.py" /tmp/closures.before.txt "$HERE/gen/out/closures.txt"
