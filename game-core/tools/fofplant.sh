#!/bin/sh
# Fight or Flight: show the comparator can fail. Replays one trace (from tools/fofdiff.sh) through copies of game-core, each with one line
# of src/gc_fof.c (or gc_encounter.c, gc_mystery.c, gc_phases.c, gc_battle.c; or battle-core's src/switching.c and src/commands.c, for
# which battle-core and run-core are copied and rebuilt) changed, and prints each one's totals. Every copy must exit 1.
# Not planted: the experience of a won fight (the oracle does not play the fight; the ME exp floor is checked by reading), the shiny variant of a boss (no trace's boss was shiny: the base chance is 1/65536 and no Shiny Charm brings it near).
# Usage: tools/fofplant.sh <trace>   (FOF_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-fof-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_fof.c}
  [ "$NAME" != "${FOF_PLANT_FROM:-}" ] || started=1
  [ -z "${FOF_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
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
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_fof_diff_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_fof_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant level-rounding 's|return base + (int)floor((double)run->wave / 10 + 0.5);|return base + (int)floor((double)run->wave / 10);|'
plant level-scaling 's|return base + (int)floor((double)run->wave / 10 + 0.5);|return base;|'
plant luck-ignored 's|f->level, gc_party_luck(run), 1, run->wave_cycle_offset|f->level, 0, 1, run->wave_cycle_offset|'
plant not-a-boss-species 's|f->level, gc_party_luck(run), 1, run->wave_cycle_offset|f->level, gc_party_luck(run), 0, run->wave_cycle_offset|'
plant second-iv-roll 's|gc_me_new_boss(run, \&r, species, f->level, false, \&f->boss)|gc_me_new_boss(run, \&r, species, f->level, true, \&f->boss)|'
plant tier-master 's|custom.tiers\[0\] = (int8_t)(w > 160 ? 4|custom.tiers[0] = (int8_t)(w > 161 ? 4|'
plant tier-rogue 's|w > 120 ? 3|w > 121 ? 3|'
plant tier-ultra 's|w > 40 ? 2 : 1|w > 41 ? 2 : 1|'
plant luck-upgrades 's|custom.allow_luck_upgrades = false;|custom.allow_luck_upgrades = true;|'
plant tm-allowed 's|if (!strstr(key, "TM_") \&\& strcmp(key, "CANDY_JAR"))|if (strcmp(key, "CANDY_JAR"))|'
plant candy-jar-allowed 's|if (!strstr(key, "TM_") \&\& strcmp(key, "CANDY_JAR"))|if (!strstr(key, "TM_"))|'
plant thresholds-skipped 's|pr_reward_regenerate_thresholds(ctx, 0, \&th);|memset(\&th, 0, sizeof th);|'
plant option-level-roll 's|(void)pr_run_get_level_for_wave(\&run->run, \&r, run->wave);|(void)0;|'
plant new-id 's|e->b.id = (uint32_t)floor(pr_rng_real_in_range(\&r, 0, 4294967296.0));|(void)r;|'
plant passive-flag 's|e->b.passive_flag = false;|e->b.passive_flag = true;|'
plant spent-boost-flag 's|e->b.me_post_summon = PR_ME_POST_STAT_BOOST;|e->b.me_post_summon = PR_ME_POST_NONE;|'
plant item-recalc 's|gc_enemy_finish_party_me(run);|gc_enemy_finish_party(run);|'
plant no-flee 's|in->me_no_flee = true;|in->me_no_flee = false;|'
plant reward-forgotten 's|in->me_reward.active = true;|in->me_reward.active = false;|'
plant shop-fill 's|c->fill_remaining = false;|c->fill_remaining = true;|'
plant steal-exp-amount 's|gc_exp_party_queue(run, \&pp, gc_form(f.boss.b.species, 0)->base_exp, true);|gc_exp_party_queue(run, \&pp, gc_form(f.boss.b.species, 0)->base_exp + 1, true);|'
plant steal-option-open 's|    if (option == 2) { gc_me_leave_with_heal_shop(run); return; }|    if (option == 2) { gc_me_end_shop(run); return; }|'
plant thief-not-stealing 's|MOVE_ID_PLUCK, MOVE_ID_COVET, MOVE_ID_KNOCK_OFF, MOVE_ID_THIEF, MOVE_ID_TRICK, MOVE_ID_SWITCHEROO};|MOVE_ID_PLUCK, MOVE_ID_COVET, MOVE_ID_KNOCK_OFF, MOVE_ID_COVET, MOVE_ID_TRICK, MOVE_ID_SWITCHEROO};|' gc_mystery.c
plant steal-needs-able '/static bool has_stealing_move/,/^}/ s|    if (!gc_mon_allowed(m)) return false;|    (void)0;|' gc_mystery.c
plant berry-cap 's|it->arg == BERRY_TYPE_SITRUS \|\| it->arg == BERRY_TYPE_LUM|it->arg == BERRY_TYPE_LUM|' gc_encounter.c
plant booster-cap 's|return run->enemy\[i\].b.ivs\[it->arg\];|return 99;|' gc_encounter.c
plant stack-cap-gone 's|if (token \|\| o->stack + 1 <= enemy_item_max_stack(run, o)) o->stack++;|o->stack++;|' gc_encounter.c
plant boost-range 's|const int32_t stat = pr_rng_seed_int(\&bt->rng, 4, 1);.*|const int32_t stat = pr_rng_seed_int(\&bt->rng, 3, 1);|' battle-core/src/switching.c
plant boost-battle-rng 's|const int32_t stat = pr_rng_seed_int(\&bt->rng, 4, 1);.*|const int32_t stat = pr_battle_rand_seed_int(bt, 4, 1);|' battle-core/src/switching.c
plant boost-stages 's|pr_unshift_stat_stage_change(bt, e, q->battler, \&stat, 1, 2, p);|pr_unshift_stat_stage_change(bt, e, q->battler, \&stat, 1, 1, p);|' battle-core/src/switching.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
