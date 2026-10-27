#!/bin/sh
# Training Session: show the comparator can fail. Replays one trace (from tools/trainingdiff.sh) through copies of game-core, each with one line
# of src/gc_training.c (or gc_encounter.c, gc_mystery.c, gc_phases.c, gc_battle.c; or battle-core's src/switching.c and src/commands.c, for
# which battle-core and run-core are copied and rebuilt) changed, and prints each one's totals. Every copy must exit 1.
# A plant counts as caught when the comparator OR tests/mystery_training_test.c (the host test of the battle itself) fails.
# Not planted (planted, and not caught, because the change has no effect): the light training's segment cap of 5 (wave 200 gives 5 anyway), the clone's full HP (the battle's setup refills it too), the game-over line for a party with nobody else able (the battle's start ends the run anyway). Also not planted: the boss's segment index, the fainted-lead swap (the battle start's), the order of the callback's messages.
# Usage: tools/trainingplant.sh <trace>   (TRN_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-training-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_training.c}
  [ "$NAME" != "${TRN_PLANT_FROM:-}" ] || started=1
  [ -z "${TRN_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  case "$FILE" in
    battle-core/*)
      for d in battle-core run-core; do mkdir -p "$W/$d"; (cd "$ROOT/$d" && tar cf - --exclude=./build .) | tar xf - -C "$W/$d"; done
      for d in ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
      TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *)
      for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
      TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_training_diff_test" "$W/build/mystery_training_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_training_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  ht=0; "$W/build/mystery_training_test" >"$W/host.txt" 2>&1 || ht=$?
  echo "$NAME: comparator exit $st, host test exit $ht, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || [ "$ht" != 0 ] || fails=$((fails + 1))
}
plant segments-moderate 's|option == 0 ? 50 : option == 1 ? 40 : 30|option == 0 ? 50 : option == 1 ? 41 : 30|'
plant pick-fainted 's|return idx >= 0 \&\& idx < run->n_party \&\& gc_mon_allowed(\&run->party\[idx\]);|return idx >= 0 \&\& idx < run->n_party;|'
plant nature-mask 's|(1u << (nature + 1))|(1u << nature)|'
plant ability-dedupe 's|if (!is_new\[j\] \&\& ability_of_slot|if (ability_of_slot|'
plant ability-count 's|f->ability_hidden == 0 ? 2 : 3|f->ability_hidden == 0 ? 3 : 3|'
plant ability-new-mark 's|ability_attr \& (1u << i))|ability_attr \& (1u << (i + 1)))|'
plant level-draw-missing 's|(void)pr_run_get_level_for_wave(\&run->run, \&r, run->wave);|(void)0;|'
plant boss-id 's|4294967296.0|4294967295.0|'
plant passive-kept 's|e->b.passive_flag = false;|e->b.passive_flag = true;|'
plant items-not-given 's|gc_enemy_finish_party_items(run, given, in->training.n_kept);|gc_enemy_finish_party_items(run, given, 0);|'
plant item-transferable 's|given\[i\].non_transferable = true;|given[i].non_transferable = false;|'
plant no-flee 's|in->me_no_flee = true;|in->me_no_flee = false;|'
plant seen-counted 's|in->me_no_stats = true;|in->me_no_stats = false;|'
plant reward-forgotten 's|in->me_reward.active = true;|in->me_reward.active = false;|'
plant reward-no-fill 's|in->me_reward.fill = true;|in->me_reward.fill = false;|'
plant reward-no-callback 's|in->me_reward.before = true;|in->me_reward.before = false;|'
plant items-not-pruned 's|    gc_me_prune_items(run);|    (void)0;|'
plant iv-one-bonus 's|if (iv <= 21 \&\& iv - 1 == 0) value += 1;|if (iv <= 21 \&\& iv - 1 == 1) value += 1;|'
plant iv-step 's|iv <= 10 ? 10 : iv <= 20 ? 5 : 3|iv <= 10 ? 10 : iv <= 20 ? 6 : 3|'
plant iv-cap 's|if (value > 31) value = 31;|if (value > 32) value = 32;|'
plant iv-count 's|improved < 2|improved < 3|'
plant iv-shuffle 's|pr_rng_seed_int(\&r, i + 1, 0)|pr_rng_seed_int(\&r, i, 0)|'
plant iv-maxed-listed 's|if (m->b.ivs\[i\] < 31) {|if (m->b.ivs[i] <= 31) {|'
plant iv-stat-order 's|if (improved == 0) stat1 = index; else stat2 = index;|if (improved == 1) stat1 = index; else stat2 = index;|'
plant iv-dex 's|gc_system_update_species_dex_ivs(run->sys, gc_species_root_for_starter(m->b.species), m->b.ivs);|(void)0;|'
plant iv-caught '/if (improved > 0) {/,/^        }/ s|gc_system_set_pokemon_caught(run->sys, m, false, false);|(void)0;|'
plant rng-stream 's|run->wave \* 1000, NULL|run->wave * 1001, NULL|'
plant nature-value 's|m->b.nature = (uint8_t)in->training.nature;|m->b.nature = (uint8_t)((in->training.nature + 1) % 25);|'
plant nature-not-custom 's|m->has_custom_nature = true;|m->has_custom_nature = false;|'
plant nature-dex 's|gc_system_unlock_species_nature(run->sys, m->b.species, (uint8_t)in->training.nature);|(void)0;|'
plant ability-index 's|m->ability_index = (uint8_t)in->training.ability;|m->ability_index = 0;|'
plant ability-value 's|m->b.ability = m->b.base_ability = (uint16_t)gc_form_ability(m->b.species, gc_form(m->b.species, m->b.form), in->training.ability);|(void)0;|'
plant message-key 's|trainingSession:option.3.finished|trainingSession:option.3.finishd|'
plant returns-at-front 's|run->party_order\[run->n_party\] = (int8_t)run->n_party;|run->party_order[run->n_party] = 0;|'
plant items-not-returned 's|run->items\[run->n_items++\] = in->training.kept\[i\];|(void)0;|'
plant battle-party-stale 's|    run->bt.n_party\[0\] = (uint8_t)run->n_party;|    (void)0;|'
plant leave-without-heal 's|if (option == 3) { gc_me_leave_with_heal_shop(run); return; }|if (option == 3) { gc_me_leave(run); return; }|'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
