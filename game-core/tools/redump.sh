#!/bin/sh
# Re-dump game-core's generated data (gen/out/gc_*.c) from an upstream checkout prepared by
# battle-core/tools/setup-upstream.sh. Usage: tools/redump.sh <upstream dir>
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$UP/test/tests/zz-dump"
for d in evolutions biome-fields starters trainers dex learnsets movegen; do
  o=$(echo "$d" | tr - _)
  cp "$HERE/gen/dump-$d.test.ts" "$UP/test/tests/zz-dump/gc-dump-$d.test.ts"
  (cd "$UP" && DUMP_OUT="$HERE/gen/out/gc_$o.c" npx vitest run "test/tests/zz-dump/gc-dump-$d.test.ts")
  echo "gen/out/gc_$o.c"
done
