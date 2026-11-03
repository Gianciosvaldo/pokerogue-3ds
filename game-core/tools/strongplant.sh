#!/bin/sh
# The Strong Stuff: show the comparator (and the host test) can fail. Replays one trace (from tools/strongdiff.sh) through copies of
# game-core, each with one line of src/gc_strong.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by
# tests/mystery_strong_diff_test.c exiting 1, or, for what the oracle does not play (the "Will you switch?" question), by tests/mystery_strong_test.c failing.
# Not planted: battle-core's post-summon handler itself (its stage change is compared through the Def and Sp. Def stages of the differential), the Shuckle's sprite and the outro text (the app's).
# Usage: tools/strongplant.sh <trace>   (STRONG_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-strong-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_strong.c}
  [ "$NAME" != "${STRONG_PLANT_FROM:-}" ] || started=1
  [ -z "${STRONG_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_strong_diff_test" "$W/build/mystery_strong_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_strong_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_strong_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
plant sort-ascending 's|base_stat_total(\&run->party\[order\[j\]\]) < base_stat_total(\&run->party\[v\])|base_stat_total(\&run->party[order[j]]) > base_stat_total(\&run->party[v])|'
plant sort-unstable 's|base_stat_total(\&run->party\[order\[j\]\]) < base_stat_total(\&run->party\[v\])|base_stat_total(\&run->party[order[j]]) <= base_stat_total(\&run->party[v])|'
plant storage-order 's|order\[n++\] = run->party_order\[k\];|order[n++] = k;|'
plant reduction-value 's|#define HIGH_BST_REDUCTION_VALUE 15|#define HIGH_BST_REDUCTION_VALUE 14|'
plant increase-value 's|#define BST_INCREASE_VALUE 10|#define BST_INCREASE_VALUE 11|'
plant high-count-one 's|const int amount = i < 2 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;|const int amount = i < 1 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;|'
plant high-count-three 's|const int amount = i < 2 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;|const int amount = i < 3 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;|'
plant juice-swapped 's|const int amount = i < 2 ? -HIGH_BST_REDUCTION_VALUE : BST_INCREASE_VALUE;|const int amount = i < 2 ? BST_INCREASE_VALUE : -HIGH_BST_REDUCTION_VALUE;|'
plant juice-stats-stale 's|        gc_me_update_modifiers(run);|        (void)0;|'
plant text-names-swapped 's|i ? "highBstPokemon2" : "highBstPokemon1"|i ? "highBstPokemon1" : "highBstPokemon2"|'
plant text-reduction 's|pr_arg_int("reductionValue", HIGH_BST_REDUCTION_VALUE)|pr_arg_int("reductionValue", BST_INCREASE_VALUE)|'
plant text-missing 's|    gc_ev_msg(run, "mysteryEncounters/theStrongStuff:option.1.selected2", 4, high\[0\], high\[1\],|    (void)high; gc_ev_msg(run, "mysteryEncounters/theStrongStuff:option.1.selected", 4, high[0], high[1],|'
plant shop-no-fill 's|    c->fill_remaining = true;|    c->fill_remaining = false;|'
plant shop-heal 's|    gc_me_end_shop(run);|    gc_me_leave_with_heal_shop(run);|'
plant level-boost 's|floor((double)run->wave / 10 \* 1.0 + 0.5), 0);|floor((double)run->wave / 10 * 0.5 + 0.5), 0);|'
plant level-rounding 's|floor((double)run->wave / 10 \* 1.0 + 0.5), 0);|floor((double)run->wave / 10 * 1.0), 0);|'
plant level-stream 's|    pr_rng r = run->run.rng;|    pr_rng r = gc_me_option_rng(run);|'
plant second-iv-roll 's|SPECIES_ID_SHUCKLE, level, true, boss)|SPECIES_ID_SHUCKLE, level, false, boss)|'
plant rng-not-kept 's|    run->run.rng = r;|    (void)r;|'
plant bars-four 's|    boss->b.boss_segments = 5;|    boss->b.boss_segments = 4;|'
plant bar-index 's|    boss->b.boss_segment_index = 4;|    boss->b.boss_segment_index = 3;|'
plant not-a-boss 's|    boss->b.is_boss = true;|    boss->b.is_boss = false;|'
plant shiny-kept 's|    boss->b.shiny = false;|    boss->b.shiny = true;|'
plant move-infestation 's|gc_mon_set_move(boss, 0, MOVE_ID_INFESTATION);|gc_mon_set_move(boss, 0, MOVE_ID_BUG_BITE);|'
plant move-salt-cure 's|gc_mon_set_move(boss, 1, MOVE_ID_SALT_CURE);|gc_mon_set_move(boss, 1, MOVE_ID_ROCK_BLAST);|'
plant move-gastro 's|gc_mon_set_move(boss, 2, MOVE_ID_GASTRO_ACID);|gc_mon_set_move(boss, 2, MOVE_ID_TOXIC);|'
plant move-heal-order 's|gc_mon_set_move(boss, 3, MOVE_ID_HEAL_ORDER);|gc_mon_set_move(boss, 3, MOVE_ID_WITHDRAW);|'
plant move-order 's|gc_mon_set_move(boss, 0, MOVE_ID_INFESTATION);|gc_mon_set_move(boss, 0, MOVE_ID_SALT_CURE);|'
plant post-summon-lost 's|    boss->b.me_post_summon = PR_ME_POST_DEF_SPDEF;|    boss->b.me_post_summon = PR_ME_POST_NONE;|'
plant post-summon-wrong 's|    boss->b.me_post_summon = PR_ME_POST_DEF_SPDEF;|    boss->b.me_post_summon = PR_ME_POST_ENRAGED_LOW;|'
plant berry-sitrus 's|BERRY_TYPE_SITRUS, .stack = 1|BERRY_TYPE_SITRUS, .stack = 2|'
plant berry-enigma 's|BERRY_TYPE_ENIGMA, .stack = 1|BERRY_TYPE_LEPPA, .stack = 1|'
plant berry-apicot 's|BERRY_TYPE_APICOT, .stack = 1|BERRY_TYPE_LANSAT, .stack = 1|'
plant berry-ganlon 's|BERRY_TYPE_GANLON, .stack = 1|BERRY_TYPE_PETAYA, .stack = 1|'
plant berry-lum-stack 's|BERRY_TYPE_LUM, .stack = 2|BERRY_TYPE_LUM, .stack = 1|'
plant items-count 's|gc_enemy_finish_party_items(run, items, 5);|gc_enemy_finish_party_items(run, items, 4);|'
plant reward-forgotten 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant reward-func 's|gc_reward_item_index("SOUL_DEW");|gc_reward_item_index("LEFTOVERS");|'
plant reward-no-func 's|    in->me_reward.func = (int16_t)gc_reward_item_index("SOUL_DEW");|    in->me_reward.func = -1;|'
plant reward-no-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant flee-allowed 's|    in->me_no_flee = true;|    in->me_no_flee = false;|'
plant switch-asked 's|    in->me_no_switch = true;|    in->me_no_switch = false;|'
plant start-gastro-missing 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_GASTRO_ACID);|    (void)0;|'
plant start-rock-missing 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_STEALTH_ROCK);|    (void)0;|'
plant start-order 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_GASTRO_ACID);|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_STEALTH_ROCK);|'
plant start-source 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_STEALTH_ROCK);|    gc_me_add_start_effect(run, 0, 0, MOVE_ID_STEALTH_ROCK);|'
plant option-swapped 's|    if (option == 0) approach(run);|    if (option == 1) approach(run);|'
plant catch-allowed 's|    case GC_ME_BERRIES_ABOUND: case GC_ME_SLUMBERING_SNORLAX: case GC_ME_FIERY_FALLOUT: case GC_ME_UNCOMMON_BREED:|    case GC_ME_BERRIES_ABOUND: case GC_ME_SLUMBERING_SNORLAX: case GC_ME_FIERY_FALLOUT: case GC_ME_UNCOMMON_BREED: case GC_ME_THE_STRONG_STUFF:|' gc_mystery.c
plant ask-switch-lost 's|    run->ask_switch = ask_switch;|    (void)ask_switch;|' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
