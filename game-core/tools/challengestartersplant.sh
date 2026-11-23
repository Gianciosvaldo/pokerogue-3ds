#!/bin/sh
# The Challenge Mode starter rules (Fresh Start): show the comparator can fail. Replays one trace (from tools/challengestartersdiff.sh) through copies of game-core, each
# with one line of src/gc_challenge.c (or gc_mon.c) changed, and prints each one's totals. Every copy must exit 1.
# Usage: tools/challengestartersplant.sh <trace>   (CHALLENGE_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-challenge-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_challenge.c}
  [ "$NAME" != "${CHALLENGE_PLANT_FROM:-}" ] || started=1
  [ -z "${CHALLENGE_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
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
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/challenge_starters_diff_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/challenge_starters_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
# the cost, the starter select's view
plant cost-keeps-reduction 's/    if (set \&\& set->value\[CHALLENGES_FRESH_START\]) return gc_starter_cost(species);/    if (set \&\& set->value[CHALLENGES_FRESH_START]) return gc_starter_value(sys, species);/'
plant view-egg-moves 's/out->egg_moves = 0; /out->egg_moves |= 0; /'
plant view-hidden-ability 's/out->ability_attr \&= 1u | 2u;/out->ability_attr \&= 1u | 2u | 4u;/'
plant view-passive 's/out->passive_attr = 0;/out->passive_attr |= 0;/'
plant view-value-reduction 's/out->value_reduction = 0;/out->value_reduction |= 0;/'
plant view-nature-mask 's/(1u << 7)/(1u << 8)/'
plant view-iv-cap 's/if (out->ivs\[i\] > 15) out->ivs\[i\] = 15;/if (out->ivs[i] > 16) out->ivs[i] = 16;/'
plant view-form-mask 's/out->caught_attr \&= 0xFF;/out->caught_attr \&= 0xFFF;/'
plant view-form-reset-species 's/species == SPECIES_ID_PIKACHU ||/species == SPECIES_ID_PICHU ||/'
plant choice-ignores-fresh-start 's/tmp.challenge_fresh_start = set->value\[CHALLENGES_FRESH_START\];/tmp.challenge_fresh_start = 0;/'
# the starter Pokemon
plant mon-ability-wrap 's/m->ability_index %= 2;/m->ability_index %= 3;/'
plant mon-passive 's/^    p->passive_flag = false;/    p->passive_flag = true;/'
plant mon-move-level 's/lm->level <= 5/lm->level <= 6/'
plant mon-form-moves 's/f->species != p->species ||/1 ||/'
plant mon-no-level-sort 's/while (j > 0 \&\& vlev\[j - 1\] > lv)/while (j > 0 \&\& 0)/'
plant mon-fill-duplicates 's/if (!have) list\[nl++\] = valid\[k\];/list[nl++] = valid[k];/'
plant mon-luck 's/m->luck = 0; /m->luck = m->luck; /'
plant mon-form-reset 's/if (p->form > 0 \&\& fresh_start_form_reset(p->species)) p->form = 0;/if (p->form > 0 \&\& 0) p->form = 0;/'
plant mon-iv-cap 's/for (int i = 0; i < 6; i++) if (p->ivs\[i\] > 15) p->ivs\[i\] = 15;/for (int i = 0; i < 6; i++) if (p->ivs[i] > 16) p->ivs[i] = 16;/'
plant mon-tera 's/if (f0) p->tera_type = f0->type1; /if (f0) p->tera_type = p->tera_type; /'
# the unchallenged starter's default tera type: the species' own, not the form's
plant default-tera-form 's/gc_form(s->species, 0)->type1)/f->type1)/' gc_mon.c
echo "challenge starters plant: $total planted, $fails not caught"
[ "$fails" = 0 ]
