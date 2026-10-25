#!/bin/sh
# The Global Trade System: show the comparator can fail. Replays one trace (from tools/gtsdiff.sh) through copies of game-core,
# each with one line of src/gc_gts.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must exit 1.
# Not planted: the Shiny roll's 4096 cap (4095 differs only when the draw is exactly 4095 of 65536: about 1 in 16000 of the shiny rolls).
# Usage: tools/gtsplant.sh <trace>   (GTS_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-gts-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_gts.c}
  [ "$NAME" != "${GTS_PLANT_FROM:-}" ] || started=1
  [ -z "${GTS_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_gts_diff_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_gts_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant bst-range 's/cap = bst + 100; min = bst - 100;/cap = bst + 99; min = bst - 100;/'
plant range-widening 's/min -= 10;/min -= 9;/'
plant shuffle-bound 's/const int j = pr_rng_integer_in_range(r, 0, i);/const int j = pr_rng_integer_in_range(r, 0, i - 1);/'
plant candidate-threshold 's/if (n > 20) {/if (n > 21) {/'
plant legendary-pool-order 's/legendary_pools\[gen\]\[j\], 5/legendary_pools[gen][2 - j], 5/'
plant already-used-growth 's/used\[n_used++\] = (uint16_t)species\[j\];/used[n_used] = (uint16_t)species[j];/'
plant form-bst 's/const pr_form \*f = gc_form(m->b.species, m->b.form);/const pr_form *f = gc_form(m->b.species, 0);/'
plant mythical-not-special 's/sg->legendary || sg->sub_legendary || sg->mythical/sg->legendary || sg->sub_legendary/'
plant paradox-exclusion 's/SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_CROWN,/SPECIES_ID_IRON_BOULDER,/'
plant shiny-threshold 's/threshold \*= pow(2, 1 + charms);/threshold *= pow(2, charms);/'
plant hidden-chance 's/chance \*= pow(2, -1 - stacks);/chance *= pow(2, -stacks);/'
plant egg-move-index 's/const int move = e->egg_moves\[pr_rng_seed_int(r, 3, 0)\];/const int move = e->egg_moves[pr_rng_seed_int(r, 4, 0) % 4];/'
plant egg-move-slot 's/const int slot = pr_rng_seed_int(r, 4, 0);/const int slot = pr_rng_seed_int(r, 3, 0);/'
plant wonder-level 's/wonder_trade_pokemon(run, run->party\[b\].b.level, \&wonder)/wonder_trade_pokemon(run, run->party[b].b.level + 1, \&wonder)/'
plant passive-from-received 's/nm.b.passive_flag = traded_passive;/nm.b.passive_flag = received->b.passive_flag;/'
plant dex-ivs-skipped 's/gc_system_update_species_dex_ivs(/if (0) gc_system_update_species_dex_ivs(/'
plant goodbye-message 's/globalTradeSystem:pokemonTradeGoodbye"/globalTradeSystem:pokemonTradeGoodbyeX"/'
plant form-item-carried '322s/"RARE_FORM_CHANGE_ITEM"/"RARE_FORM_CHANGE_ITEMX"/'
plant species-booster-carried 's/|| !strcmp(it->key, "SPECIES_STAT_BOOSTER") || !strcmp(it->key, "RARE_SPECIES_STAT_BOOSTER")) continue;/|| !strcmp(it->key, "SPECIES_STAT_BOOSTER")) continue;/'
plant rare-form-transferable '187s/"RARE_FORM_CHANGE_ITEM"/"RARE_FORM_CHANGE_ITEMX"/'
plant tier-raise 's/if (tier < 4) tier++;/if (tier < 3) tier++;/'
plant white-herb-tier 's/if (!strcmp(chosen.key, "WHITE_HERB")) tier = 1;/if (!strcmp(chosen.key, "WHITE_HERB")) tier = 0;/'
plant lucky-egg-tier 's/else if (!strcmp(chosen.key, "LUCKY_EGG")) tier = 2;/else if (!strcmp(chosen.key, "LUCKY_EGG")) tier = 1;/'
plant tm-reroll 's/made = strstr(pr_item_keys\[got.type.item\], "TM_") == NULL;/made = true;/'
plant luck-upgrades 's/custom.allow_luck_upgrades = false;/custom.allow_luck_upgrades = true;/'
plant item-trade-name-rng 's/const char \*trader = trader_name(&run->run.rng);/const char *trader = trader_name((pr_rng[1]){run->run.rng});/'
plant item-stack-kept 's/if (--run->items\[held\[which\]\].stack <= 0)/if (--run->items[held[which]].stack < 0)/'
plant trader-gender 's/list = pick_index(r, 2) == 0 ? 1 : 2;/list = pick_index(r, 2) == 0 ? 2 : 1;/'
plant item-pick-filter 's/return gc_gts_held_items(run, idx, held) > 0;/return gc_gts_held_items(run, idx, held) >= 0;/'
plant two-allowed 's/s->option_ok\[0\] = s->option_ok\[1\] = gc_gts_allowed(run) >= 2;/s->option_ok[0] = s->option_ok[1] = gc_gts_allowed(run) >= 1;/' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
