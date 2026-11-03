#!/bin/sh
# The Endless starter rules: show the comparator can fail. Replays one trace (from tools/startersdiff.sh) through copies of game-core, each
# with one line of src/gc_run.c (or gc_mon.c) changed, and prints each one's totals. Every copy must exit 1.
# Usage: tools/startersplant.sh <trace>   (STARTERS_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-starters-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_run.c}
  [ "$NAME" != "${STARTERS_PLANT_FROM:-}" ] || started=1
  [ -z "${STARTERS_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  # the other cores are not changed: start from the libraries already built (make rebuilds what is out of date)
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/starters_diff_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/starters_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
# the cost limit
plant budget-endless-10 's/mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS ? 15 : 10/mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS ? 10 : 10/' gc_mon.c
plant budget-classic-15 's/mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS ? 15 : 10/mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS ? 15 : 15/' gc_mon.c
plant budget-spliced-10 's/mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS ? 15 : 10/mode == GAME_MODES_ENDLESS ? 15 : 10/' gc_mon.c
# which party is fused, and how
plant fused-in-endless 's/if (run->run.mode == GAME_MODES_SPLICED_ENDLESS) {/if (run->run.mode == GAME_MODES_ENDLESS) {/'
plant fused-in-classic 's/if (run->run.mode == GAME_MODES_SPLICED_ENDLESS) {/if (run->run.mode != GAME_MODES_ENDLESS) {/'
plant no-constructor-fusion 's/if (!(constructor_fusion_draws(run, \&run->party\[i\]) \&\& gc_mon_starter_fusion(run, \&run->party\[i\])))/if (!gc_mon_starter_fusion(run, \&run->party[i]))/'
plant filter-prevolution '393s/c->has_prevolution/0/'
plant filter-no-evolutions '393s/!c->has_evolutions/0/'
plant filter-sublegendary '393s/c->sub_legendary/0/'
# Meltan is the one unevolved, evolving species the filter drops as both mythical and uncatchable: only dropping both clauses shows.
# (legendary, trainer-forbidden and Ditto are never unevolved-with-evolutions in the data: those guards are upstream's, kept, and have no observable mutant.)
plant filter-mythical-and-uncatchable '393s/!c->catchable/0/;393s/c->mythical/0/'
plant filter-self '394s/c->id == m->b.species/0/'
plant pick-first 's/list\[n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1)\]/list[0]/'
plant hidden-rate 's/#define GC_BASE_HIDDEN_ABILITY_RATE 256/#define GC_BASE_HIDDEN_ABILITY_RATE 128/'
plant ability-index-swapped 's/fs->same_abilities ? 0 : rand_ability/fs->same_abilities ? 0 : 1 - rand_ability/'
plant ability-hidden-never 's/fs->has_hidden \&\& has_hidden ? 2/fs->has_hidden \&\& 0 ? 2/'
plant gender-threshold '408s/0.390625/0.39/'
plant gender-inverted 's/? GENDER_MALE : GENDER_FEMALE);/? GENDER_FEMALE : GENDER_MALE);/'
plant fusion-shiny 's/m->fusion_shiny = p->shiny;/m->fusion_shiny = false;/'
plant fusion-variant 's/m->fusion_variant = m->variant;/m->fusion_variant = 0;/'
plant fusion-luck 's/m->fusion_luck = m->luck;/m->fusion_luck = 0;/'
plant fusion-form-arena 's/pr_run_get_egg_form_index(rng, \&run->run, \&ctx, p->fusion_gender)/0/'
plant stats-not-recalculated 's/^    pr_calculate_stats(\&run->bt, p);$/    (void)0;/'
plant tera-draw-skipped 's/if (n > 1) (void)pr_rng_integer_in_range(\&run->run.rng, 0, n - 1);/(void)0;/'
plant tera-draw-always 's/if (n > 1) (void)pr_rng_integer_in_range(\&run->run.rng, 0, n - 1);/(void)pr_rng_integer_in_range(\&run->run.rng, 0, n);/'
plant plain-tera-draw-skipped 's/if (!constructor_tera_draw(run, \&plain, run->party\[i\].b.species)) gc_stub(run, GC_STUB_UNPORTED_ENEMY);/(void)plain;/'
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
