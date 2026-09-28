#!/bin/sh
# Generate N upstream cases and replay them through run-core.
# Usage: tools/difftest.sh <upstream dir> [N] [seed] [wave|shop|biome|encounter|newbattle|exp|partyexp|upkeep|battlegen|pokemongen|trainerspecies|trainerparty|wildenemy|luck|enemyitems|enemybuffs|all]
# The upstream checkout is the one battle-core/tools/setup-upstream.sh prepares. pokemongen reads the
# variant masterlist tools/redump.sh fetches (it runs redump's fetch if the file isn't there yet).
set -e
UP=${1:?upstream dir}; N=${2:-20000}; SEED=${3:-12345}; WHAT=${4:-all}
HERE=$(cd "$(dirname "$0")/.." && pwd)
BUILD=${BUILD:-/tmp/run-core-build}
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
ASSETS=$(git -C "$UP" ls-tree HEAD assets | awk '{print $3}')
ML="$UP/test/tests/zz-dump/variant-masterlist-$ASSETS.json"
if [ "$WHAT" = all ] || [ "$WHAT" = pokemongen ] || [ "$WHAT" = wildenemy ]; then
  [ -s "$ML" ] || curl -sSf -o "$ML" "https://raw.githubusercontent.com/pagefaultgames/pokerogue-assets/$ASSETS/images/pokemon/variant/_masterlist.json"
fi
for t in wave shop biome encounter newbattle exp partyexp upkeep battlegen pokemongen trainerspecies trainerparty wildenemy luck enemyitems enemybuffs; do
  [ "$WHAT" = all ] || [ "$WHAT" = $t ] || continue
  cp "$HERE/tests/$t.producer.test.ts" "$UP/test/tests/zz-dump/"
  (cd "$UP" && VARIANT_MASTERLIST="$ML" DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT=$BUILD/$t-cases.txt npx vitest run test/tests/zz-dump/$t.producer.test.ts >/dev/null)
  echo "== $t"
  "$BUILD/${t}test" "$BUILD/$t-cases.txt"
done
