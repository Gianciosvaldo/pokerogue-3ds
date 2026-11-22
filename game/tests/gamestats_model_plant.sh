#!/bin/sh
# The Game Stats screen's model: show gamestats_model_test can fail. Each plant changes one line of a copy of source/gamestats_model.c, rebuilds the
# test with it and runs it on the fixture; every copy must exit 1.
# Usage: tests/gamestats_model_plant.sh [fixture]   (from any folder; the fixture defaults to fixtures/game-stats-screen.native.txt.gz)
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
FIX=${1:-$HERE/fixtures/game-stats-screen.native.txt.gz}
WORK=${WORK:-/tmp/gamestats-model-plant}
fails=0; total=0
rm -rf "$WORK"; mkdir -p "$WORK"
plant() {
  NAME=$1; EXPR=$2
  cp "$HERE/../source/gamestats_model.c" "$WORK/gamestats_model.c"
  sed -i "$EXPR" "$WORK/gamestats_model.c"
  n=$(diff "$HERE/../source/gamestats_model.c" "$WORK/gamestats_model.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$HERE" GSM_MODEL="$WORK/gamestats_model.c" BUILD_OUT="$WORK/t" "$WORK/t" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$WORK/t" "$FIX" >"$WORK/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$WORK/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant percent-rounds-not-floors   's/(int)floor((double)count/(int)round((double)count/'
plant tofixed-tie-rounds-even     "s/dot\[4\] >= '5'/dot[4] > '5'/"
plant shiny-starters-any-caught   's/if (shiny ? (a \& DEX_SHINY) != 0 : a != 0) c++;/if (a != 0) c++;/'
plant hidden-shown-at-one         's/return r->hidden \&\& strtol(r->value, NULL, 10) == 0;/return r->hidden \&\& strtol(r->value, NULL, 10) == 1;/'
plant seen-ignores-caught         's/(d->seen_attr != 0 || d->caught_attr != 0)/(d->seen_attr != 0)/'
plant fought-uses-seen-attr       's/kind == 0 ? d->seen_count != 0/kind == 0 ? d->seen_attr != 0/'
plant play-time-days-one-digit    's/"%02u:%02u:%02u:%02u"/"%u:%02u:%02u:%02u"/'
plant caught-wrong-denominator    's/count_percent(species_where(sys, 2), gc_species_dex_info_count/count_percent(species_where(sys, 2), gc_starter_cost_count/'
plant damage-row-reads-heal       's/PLAIN("highestDamage", s->highest_damage, false);/PLAIN("highestDamage", g->highest_heal, false);/'
plant endless-runs-not-hidden     's/PLAIN("endlessRuns", g->endless_sessions_played, true);/PLAIN("endlessRuns", g->endless_sessions_played, false);/'
plant million-threshold-off       's/n >= 1000000u ? 2/n >= 1000001u ? 2/'
plant hatched-reads-per-egg       's/PLAIN("eggsHatched", g->pokemon_hatched, false);/PLAIN("eggsHatched", s->total_hatches, false);/'
plant manaphy-reads-legendary     's/PLAIN("manaphyEggsPulled", s->manaphy_eggs_pulled, true);/PLAIN("manaphyEggsPulled", s->legendary_eggs_pulled, true);/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
