#!/bin/sh
# Re-dump run-core's generated data (gen/out/run_*.c) from an upstream checkout prepared by
# battle-core/tools/setup-upstream.sh. Usage: tools/redump.sh <upstream dir>
# pokemongen also reads the variant masterlist from the assets repo, at the commit upstream pins
# (fetched once into the upstream checkout).
set -e
UP=${1:?upstream dir}
HERE=$(cd "$(dirname "$0")/.." && pwd)
ASSETS=$(git -C "$UP" ls-tree HEAD assets | awk '{print $3}')
ML="$UP/test/tests/zz-dump/variant-masterlist-$ASSETS.json"
mkdir -p "$UP/test/tests/zz-dump"
[ -s "$ML" ] || curl -sSf -o "$ML" "https://raw.githubusercontent.com/pagefaultgames/pokerogue-assets/$ASSETS/images/pokemon/variant/_masterlist.json"
for d in biomes evolutions exp trainers pokemongen trainerspecies; do
  cp "$HERE/gen/dump-$d.test.ts" "$UP/test/tests/zz-dump/"
  (cd "$UP" && VARIANT_MASTERLIST="$ML" DUMP_OUT="$HERE/gen/out/run_$d.c" npx vitest run "test/tests/zz-dump/dump-$d.test.ts" >/dev/null)
  echo "gen/out/run_$d.c"
done
