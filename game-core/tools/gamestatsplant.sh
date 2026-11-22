#!/bin/sh
# The Game Stats recording: show the comparator can fail. Replays one trace (from tools/gamestatsdiff.sh) through copies of game-core, each with one
# line of src/gc_system.c changed, and prints each one's totals. Every copy must exit 1.
# Usage: tools/gamestatsplant.sh <trace>   (GAMESTATS_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-gamestats-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_system.c}
  [ "$NAME" != "${GAMESTATS_PLANT_FROM:-}" ] || started=1
  [ -z "${GAMESTATS_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/gamestats_diff_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/gamestats_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
# seen
plant seen-trainer-counted 's/if (!trainer) count_rarity(sys, m, 0);/count_rarity(sys, m, 0);/'
plant seen-rarity-not-counted 's/if (!trainer) count_rarity(sys, m, 0);/(void)trainer;/'
plant seen-count-when-not-incrementing 's/if (increment_count) {\r\?$/if (increment_count || trainer) {/' 
plant seen-total-not-counted 's/if (sys->gstats.pokemon_seen < UINT32_MAX) sys->gstats.pokemon_seen++;/(void)0;/'
# the rarity chain and the shiny rule
plant mythical-as-legendary 's/rarity \& 4 ? myth\[kind\] : NULL/rarity \& 4 ? leg[kind] : NULL/'
plant sub-legendary-as-legendary 's/uint32_t \*c = rarity \& 1 ? sub\[kind\]/uint32_t *c = rarity \& 1 ? leg[kind]/'
plant fusion-half-shine-ignored 's/(m->b.shiny || (m->b.fusion_species \&\& m->fusion_shiny))/(m->b.shiny)/'
# (not planted: `m->b.shiny || m->fusion_shiny` without the fusion-species test is the same on every trace, a fusion half's shine is only ever set on a fusion)
# caught and hatched
plant hatched-total-not-counted 's/if (sys->gstats.pokemon_hatched < UINT32_MAX) sys->gstats.pokemon_hatched++;/(void)0;/'
plant hatched-rarity-as-caught 's/count_rarity(sys, m, 2);/count_rarity(sys, m, 1);/'
plant caught-rarity-as-seen 's/count_rarity(sys, m, 1);/count_rarity(sys, m, 0);/'
plant caught-only-the-caught-stage 's/if (sys->stats.total_catches < UINT32_MAX) sys->stats.total_catches++;/if (species_id == m->b.species) sys->stats.total_catches++;/'
plant caught-total-not-counted 's/if (sys->stats.total_catches < UINT32_MAX) sys->stats.total_catches++;/(void)0;/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
