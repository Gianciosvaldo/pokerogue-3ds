#!/bin/sh
# The moveset generator: show the comparator can fail. Replays one trace (from tools/movegendiff.sh) through copies of game-core,
# each with one line of src/gc_movegen.c changed, and prints each one's totals. Every copy must exit 1.
# The bosses (the trace's B and F records) have their own: the pool filters, the weight, the passive.
# Not planted, because no trace reaches them: DefAtkAttr (Body Press is in no wild level pool), Weather Ball's sandstorm type (no Rock/Ground learner).
# A plant counts as caught when any of the traces fails (a few of them need a particular roll, which one seed may not reach).
# Usage: tools/movegenplant.sh <trace> [<trace>...]
set -e
: "${1:?trace}"
TRACES=""
for t in "$@"; do TRACES="$TRACES $(CDPATH= cd -- "$(dirname "$t")" && pwd)/$(basename "$t")"; done
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-movegen-plant}
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_movegen.c}
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/movegen_diff_test" >/dev/null 2>&1
  st=0; tot=""
  for TRACE in $TRACES; do
    s1=0; "$W/build/movegen_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || s1=$?
    tot="$tot $(grep '^TOTAL' "$W/out.txt" | tr -d '\n')"
    [ "$s1" = 1 ] && st=1
  done
  echo "$NAME: exit $st,$tot"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant level-weight-offset 's/double weight = learn_level + gc_mg_const.base_level_weight_offset;/double weight = learn_level + gc_mg_const.base_level_weight_offset + 1;/'
plant evolution-weight 's/if (learn_level == 0) weight = gc_mg_const.evolution_move_weight;/if (learn_level == 0) weight = gc_mg_const.relearn_move_weight;/'
plant relearn-threshold 's/pr_moves\[id\].power >= gc_mg_const.evo_move_bp_threshold/pr_moves[id].power > gc_mg_const.evo_move_bp_threshold/'
plant max-power-start 's/double max_power = 40;/double max_power = 41;/'
plant power-floor 's/fmax(0.25, fmin(1, effective_power/fmax(0.2, fmin(1, effective_power/'
plant worse-category-ratio 's/js_pow(lower \/ higher, 3) \* 2, 1)/js_pow(lower \/ higher, 3) * 3, 1)/'
plant base-weight-ceil 's/base.e\[base.n++\].w = ceil(/base.e[base.n++].w = floor(/'
plant stab-after-signature 's/|| !mg_has_type(&c, movegen_type(&c, signature)))/|| mg_has_type(\&c, movegen_type(\&c, signature)))/'
plant signature-chance '0,/>= gc_mg_const.forced_signature_move_chance/s//> gc_mg_const.forced_signature_move_chance/'
plant walk-greater-equal 's/while (index < n - 1 \&\& rand > pool\[index\].w)/while (index < n - 1 \&\& rand >= pool[index].w)/'
plant filter-skipped 's/} while (outer > 0 \&\& filter_useless_moves(&c));/} while (0 \&\& outer > 0 \&\& filter_useless_moves(\&c));/'
plant rain-dance-abilities 's/for (int i = 0; i < 4; i++) if (mg_ability_is(c, abilities\[i\])) return false;/for (int i = 0; i < 3; i++) if (mg_ability_is(c, abilities[i])) return false;/'
plant solar-power-category 's/(pr_moves\[move\].category == MOVE_CATEGORY_SPECIAL \&\& solar_power)/(pr_moves[move].category == MOVE_CATEGORY_PHYSICAL \&\& solar_power)/'
plant weather-moves-count 's/if (weather_moves >= 2) {/if (weather_moves >= 3) {/'
plant multihit-six 's/effective = mv->power \* power_mult \* 6 \* acc_mult;/effective = mv->power * power_mult * 5 * acc_mult;/'
plant accuracy-cap 's/fmin(mv->accuracy \* acc_mult, 100)/fmin(mv->accuracy * acc_mult, 101)/'
plant delayed-turns 's/if (mg_flag(move, GC_MGF_DELAYED)) turns += 2;/if (mg_flag(move, GC_MGF_DELAYED)) turns += 1;/'
plant weather-row-ignored 's/if (!alt \&\& c->bt.weather) alt = find_pow(ab, move, 1, c->bt.weather);/if (0 \&\& !alt \&\& c->bt.weather) alt = find_pow(ab, move, 1, c->bt.weather);/'
plant gender-row-ignored 's/if (c->p.gender == 0 || c->p.gender == 1) alt = find_pow(ab, 0, 2, c->p.gender);/if (0 \&\& (c->p.gender == 0 || c->p.gender == 1)) alt = find_pow(ab, 0, 2, c->p.gender);/'
plant neuroforce-threshold 's/>= 2) \*power_mult = 1.25;/> 2) *power_mult = 1.25;/'
plant sketch-count 's/for (int i = 0; i < 4; i++) c.moves\[i\] = MOVE_ID_SKETCH;/for (int i = 0; i < 3; i++) c.moves[i] = MOVE_ID_SKETCH;/'
plant weather-ball-rain 's/case WEATHER_TYPE_RAIN: case WEATHER_TYPE_HEAVY_RAIN: type = POKEMON_TYPE_WATER; break;/case WEATHER_TYPE_RAIN: case WEATHER_TYPE_HEAVY_RAIN: type = POKEMON_TYPE_GRASS; break;/'
plant boss-weight-extra 's/if (c.boss) weight_multiplier += gc_mg_const.boss_extra_weight_multiplier;/if (0) weight_multiplier += gc_mg_const.boss_extra_weight_multiplier;/'
plant boss-superceded 's/if (c.boss) filter_superceded(&pool);/if (0) filter_superceded(\&pool);/'
plant boss-singles-only 's/((c->single \&\& sorted_has(/((!c->single \&\& sorted_has(/'
plant boss-level-denylist 's/c->p.level >= gc_mg_const.level_based_denylist_threshold/c->p.level > gc_mg_const.level_based_denylist_threshold/'
plant boss-worse-stat 's/denied >= 0 \&\& matches_offensive_category(move, denied)/denied > 0 \&\& matches_offensive_category(move, denied)/'
plant boss-pain-split 's/drop = mg_flag(move, GC_MGF_SACRIFICIAL | GC_MGF_HP_SPLIT)/drop = mg_flag(move, GC_MGF_SACRIFICIAL)/'
plant boss-terrain-setter 's/(mg_flag(move, GC_MGF_TERRAIN_CHANGE) \&\& block_terrain)/(mg_flag(move, GC_MGF_TERRAIN_CHANGE) \&\& 0)/'
plant boss-gorilla-tactics 's/|| (gorilla \&\& pr_moves\[move\].category == MOVE_CATEGORY_STATUS);/|| (0 \&\& gorilla \&\& pr_moves[move].category == MOVE_CATEGORY_STATUS);/'
plant boss-soft-fallback 's/if (pool.n == 0 \&\& unfiltered.n > 0) {/if (0 \&\& pool.n == 0 \&\& unfiltered.n > 0) {/'
plant boss-passive-ignored 's/c->passive = pr_has_passive(&c->bt, &c->p) ? c->p.passive : 0;/c->passive = 0;/'
plant boss-passive-same-ability 's/if (c->passive \&\& c->passive != c->ability) {/if (c->passive) {/'
plant boss-passive-weather-ball 's/w = gc_mg_abilities\[c->passive\].summon_weather;   \/\*/w = w;   \/*/'
plant boss-levitate-terrain 's/|| ((mg_bits_of(ab) \& GC_MGA_TERRAIN_SUMMON) \&\& (mg_ability_is(c, ABILITY_ID_LEVITATE)/|| ((mg_bits_of(ab) \& GC_MGA_TERRAIN_SUMMON) \&\& (0 \&\& mg_ability_is(c, ABILITY_ID_LEVITATE)/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
