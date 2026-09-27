#!/bin/sh
# Re-dump run-core's generated data (gen/out/run_*.c) from an upstream checkout prepared by
# battle-core/tools/setup-upstream.sh. Usage: tools/redump.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
for d in biomes evolutions exp; do
  cp "$HERE/gen/dump-$d.test.ts" "$UP/test/tests/zz-dump/"
  (cd "$UP" && DUMP_OUT="$HERE/gen/out/run_$d.c" npx vitest run "test/tests/zz-dump/dump-$d.test.ts" >/dev/null)
  echo "gen/out/run_$d.c"
done
