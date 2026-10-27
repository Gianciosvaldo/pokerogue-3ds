#!/bin/sh
# Slumbering Snorlax: show the comparator can fail. Replays one trace (from tools/snorlaxdiff.sh) through copies of game-core, each with one line
# of src/gc_snorlax.c (or gc_encounter.c, gc_mystery.c, gc_phases.c, gc_battle.c; or battle-core's src/switching.c and src/commands.c, for
# which battle-core and run-core are copied and rebuilt) changed, and prints each one's totals. Every copy must exit 1.
# A plant counts as caught when the comparator OR tests/mystery_snorlax_test.c (the host test of the battle itself) fails.
# Not planted: the Snore's IGNORE_PP use mode (Snorlax does not know Snore, so no PP is observable), the experience of a won fight (the oracle does not play the fight), the flinch lapse of the cleanup phase (the Snore flinches 30% of the time, which no fixed seed of the host test is guaranteed to hit), the boss's sprite scale.
# Usage: tools/snorlaxplant.sh <trace>   (SNO_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-snorlax-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_snorlax.c}
  [ "$NAME" != "${SNO_PLANT_FROM:-}" ] || started=1
  [ -z "${SNO_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
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
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_snorlax_diff_test" "$W/build/mystery_snorlax_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_snorlax_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  ht=0; "$W/build/mystery_snorlax_test" >"$W/host.txt" 2>&1 || ht=$?
  echo "$NAME: comparator exit $st, host test exit $ht, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || [ "$ht" != 0 ] || fails=$((fails + 1))
}
plant level-rounding 's|floor((double)run->wave / 10 \* 0.5 + 0.5)|floor((double)run->wave / 10 * 0.5)|'
plant level-modifier 's|floor((double)run->wave / 10 \* 0.5 + 0.5)|floor((double)run->wave / 10 * 0.6 + 0.5)|'
plant second-iv-roll 's|gc_me_new_boss(run, \&r, SPECIES_ID_SNORLAX, level, true, boss)|gc_me_new_boss(run, \&r, SPECIES_ID_SNORLAX, level, false, boss)|'
plant sleep-turns 's|boss->b.status_sleep_turns = 6;|boss->b.status_sleep_turns = 5;|'
plant not-asleep 's|boss->b.status = STATUS_EFFECT_SLEEP;|boss->b.status = STATUS_EFFECT_NONE;|'
plant nature 's|boss->b.nature = NATURE_DOCILE;|boss->b.nature = NATURE_HARDY;|'
plant shiny-lock 's|boss->b.shiny = false;|boss->b.shiny = true;|'
plant passive-flag 's|boss->b.passive_flag = false;|boss->b.passive_flag = true;|'
plant move-sleep-talk 's|gc_mon_set_move(boss, 2, MOVE_ID_SLEEP_TALK);|gc_mon_set_move(boss, 2, MOVE_ID_SNORE);|'
plant move-rest 's|gc_mon_set_move(boss, 3, MOVE_ID_REST);|gc_mon_set_move(boss, 3, MOVE_ID_SNORE);|'
plant berry-enigma 's|BERRY_TYPE_ENIGMA|BERRY_TYPE_LUM|'
plant booster-stat 's|.arg = STAT_HP, .stack = 1|.arg = STAT_ATK, .stack = 1|'
plant zero-stack-bell 's|if (soothe > 0) out|if (soothe >= 0) out|'
plant zero-stack-egg 's|if (egg > 0) out|if (egg >= 0) out|'
plant egg-roll-range 's|const int egg = pr_rng_seed_int(\&r, 2, 0);|const int egg = pr_rng_seed_int(\&r, 3, 0);|'
plant bell-roll-range 's|const int soothe = pr_rng_seed_int(\&r, 2, 0);|const int soothe = pr_rng_seed_int(\&r, 3, 0);|'
plant no-flee 's|in->me_no_flee = true;|in->me_no_flee = false;|'
plant reward-forgotten 's|in->me_reward.active = true;|in->me_reward.active = false;|'
plant reward-no-fill 's|in->me_reward.fill = true;|in->me_reward.fill = false;|'
plant reward-wrong-item 's|in->me_reward.func = (int16_t)gc_reward_item_index("LEFTOVERS");|in->me_reward.func = (int16_t)gc_reward_item_index("SHELL_BELL");|'
plant start-move 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_SNORE);|gc_me_add_start_effect(run, 2, 0, MOVE_ID_TACKLE);|'
plant start-source 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_SNORE);|gc_me_add_start_effect(run, 0, 0, MOVE_ID_SNORE);|'
plant start-target 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_SNORE);|gc_me_add_start_effect(run, 2, 2, MOVE_ID_SNORE);|'
plant start-effect-missing 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_SNORE);|    (void)0;|'
plant rest-message 's|option.2.restResult|option.2.restresult|'
plant rest-no-heal 's|gc_rq_push(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));|(void)0;|'
plant rest-heal-shop 's|        gc_me_leave(run);|        gc_me_leave_with_heal_shop(run);|'
plant steal-fill 's|reward_funcs(\&gc_int(run)->reward.me_custom, false);|reward_funcs(\&gc_int(run)->reward.me_custom, true);|'
plant steal-exp-to-thief 's|gc_me_steal_exp(run, idx, SPECIES_ID_SNORLAX);|gc_me_steal_exp(run, gc_mystery_primary_pokemon(run, GC_ME_SLUMBERING_SNORLAX, 2), SPECIES_ID_SNORLAX);|'
plant steal-exp-fainted-lead 's|if (!gc_mon_allowed(\&run->party\[idx\])) continue;|(void)0;|'
plant steal-needs-able '/static bool has_stealing_move/,/^}/ s|    if (!gc_mon_allowed(m)) return false;|    (void)0;|' gc_mystery.c
plant snore-never-queued 's|pr_unshift_move_phase(bt, e, source, bt->me_start\[i\].move, -1, \&target, 1, bt->me_start\[i\].use_mode, 0);|(void)target;|' battle-core/src/engine.c
plant effects-copy 's|bt->me_start\[k\].move = gc_int(run)->me_start\[k\].move;|bt->me_start[k].move = MOVE_ID_TACKLE;|' gc_battle.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
