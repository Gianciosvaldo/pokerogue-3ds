#!/bin/sh
# Delibird-y: show the comparator can fail. Replays one trace (from tools/delibirddiff.sh) through copies of game-core, each with one line of
# src/gc_delibirdy.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must exit 1.
# Not planted (equivalent under the producer): a lead that is not allowed in battle getting the Shell Bell on the Give Money / Give Food branches
# (upstream throws there: the producer never builds that case), the clamp of the money at 0 (the option is off without the price), and the
# explicit Tera Shard / vitamin / Shuckle's juice exclusions of option 3 (upstream names them; the vitamin and the juice are not transferable
# anyway, which gc_gts_item_transferable already says, and the Tera Shard is never held: removing them changes nothing).
# Usage: tools/delibirdplant.sh <trace>   (DELI_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-delibirdy-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_delibirdy.c}
  [ "$NAME" != "${DELI_PLANT_FROM:-}" ] || started=1
  [ -z "${DELI_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
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
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_delibirdy_diff_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/mystery_delibirdy_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant price-multiplier 's/#define DELIBIRDY_MONEY_PRICE_MULTIPLIER 2.0/#define DELIBIRDY_MONEY_PRICE_MULTIPLIER 1.5/'
plant money-kept 's/run->money - gc_delibirdy_price(run)/run->money - 0/'
plant money-gate 's/return run->money >= pr_run_get_wave_money_amount(run->wave, DELIBIRDY_MONEY_PRICE_MULTIPLIER);/return run->money > pr_run_get_wave_money_amount(run->wave, DELIBIRDY_MONEY_PRICE_MULTIPLIER);/'
plant seed-not-food 's/return !strcmp(it->key, "BERRY") || !strcmp(it->key, "REVIVER_SEED");/return !strcmp(it->key, "BERRY");/'
plant food-in-option3 's/return gc_gts_item_transferable(it) \&\& !food_item(it) \&\&/return gc_gts_item_transferable(it) \&\&/'
plant locked-food 's/return gc_gts_item_transferable(it) \&\& food_item(it);/return food_item(it);/'
plant locked-item 's/return gc_gts_item_transferable(it) \&\& !food_item(it)/return !food_item(it)/'
plant stack-full-over 's/gc_item_stack(run, key) >= pr_items\[idx\].max_stack;/gc_item_stack(run, key) > pr_items[idx].max_stack;/'
plant stack-full-early 's/gc_item_stack(run, key) >= pr_items\[idx\].max_stack;/gc_item_stack(run, key) >= pr_items[idx].max_stack - 1;/'
plant candy-pouch-swapped 's/!strcmp(chosen.key, "BERRY") ? "CANDY_JAR" : "BERRY_POUCH"/!strcmp(chosen.key, "BERRY") ? "BERRY_POUCH" : "CANDY_JAR"/'
plant option3-gift 's/gift(run, "HEALING_CHARM",/gift(run, "AMULET_COIN",/'
plant option1-gift 's/gift(run, "AMULET_COIN", field_lead(run));/gift(run, "HEALING_CHARM", field_lead(run));/'
plant option3-bell-holder 's/gift(run, "HEALING_CHARM", run->n_party > 0 ? \&run->party\[run->party_order\[0\]\] : NULL);/gift(run, "HEALING_CHARM", run->n_party > 1 ? \&run->party[run->party_order[1]] : NULL);/'
plant option3-bell-allowed 's/gift(run, "HEALING_CHARM", run->n_party > 0 ? \&run->party\[run->party_order\[0\]\] : NULL);/gift(run, "HEALING_CHARM", field_lead(run));/'
plant bell-over-max 's/if (!held || held < gc_item_max_held(run, key, -1, to)) {/if (!held || held <= gc_item_max_held(run, key, -1, to)) {/'
plant bell-under-max 's/if (!held || held < gc_item_max_held(run, key, -1, to)) {/if (!held || held < gc_item_max_held(run, key, -1, to) - 1) {/'
plant bell-message 's/gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", key));/(void)0;/'
plant bell-message-item 's/gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", key));/gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", "LEFTOVERS"));/'
plant bell-prune 's/        gc_me_prune_items(run);/        (void)0;/'
plant item-kept 's/if (--it->stack <= 0) gc_item_remove_at(run, i);/if (it->stack <= 0) gc_item_remove_at(run, i);/'
plant whole-stack-lost 's/if (--it->stack <= 0) gc_item_remove_at(run, i);/if (--it->stack <= 0 || 1) gc_item_remove_at(run, i);/'
plant spawn-option3-only 's/return any_pick_ok(run, 1) || any_pick_ok(run, 2);/return any_pick_ok(run, 2);/'
plant spawn-option2-only 's/return any_pick_ok(run, 1) || any_pick_ok(run, 2);/return any_pick_ok(run, 1);/'
plant pick-holder 's/if (it->pokemon_id != id) continue;/if (it->pokemon_id != id \&\& it->pokemon_id != run->party[0].b.id) continue;/'
plant option-screen-stand-in 's/for (int o = 0; o < 3; o++) s->option_ok\[o\] = gc_delibirdy_option_ok(run, o);/for (int o = 0; o < 3; o++) s->option_ok[o] = run->n_items > 0;/' gc_mystery.c
plant four-options 's/case GC_ME_AN_OFFER_YOU_CANT_REFUSE: case GC_ME_PILLAR_MEN: case GC_ME_BERRIES_ABOUND: case GC_ME_DELIBIRDY:/case GC_ME_AN_OFFER_YOU_CANT_REFUSE: case GC_ME_PILLAR_MEN: case GC_ME_BERRIES_ABOUND:/' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
