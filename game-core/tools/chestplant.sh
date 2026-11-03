#!/bin/sh
# The Mysterious Chest: show the comparator (and the host test) can fail. Replays one trace (from tools/chestdiff.sh) through copies of
# game-core, each with one line of src/gc_chest.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by
# tests/mystery_chest_diff_test.c exiting 1, or, for what the oracle does not play (the "Will you switch?" question, the SWITCH style kept
# afterwards), by tests/mystery_chest_test.c failing.
# Not planted: the form-zero stats and HP repair (Gimmighoul's constructor always draws form 0, so nothing differs: 600 cases x 4 seeds saw only form 0), the phase queue cleared before the game over (empty at that point in the harness), the luck upgrades of the tier shop (reward-core's, tested by its own differentials), the chest sprite and its animation (the app's), and the trap's text order on screen (shown by reading: showEncounterText comes at once, the
# queued "battle:fainted" after it).
# Usage: tools/chestplant.sh <trace>   (CHEST_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-chest-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_chest.c}
  [ "$NAME" != "${CHEST_PLANT_FROM:-}" ] || started=1
  [ -z "${CHEST_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_chest_diff_test" "$W/build/mystery_chest_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_chest_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_chest_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
plant roll-length 's|#define CHEST_RAND_LENGTH 100|#define CHEST_RAND_LENGTH 101|'
plant roll-offset 's|run->wave \* 1000 + 512, NULL);|run->wave * 1000 + 511, NULL);|'
plant roll-min 's|pr_rng_seed_int(\&pre, CHEST_RAND_LENGTH, 0);|pr_rng_seed_int(\&pre, CHEST_RAND_LENGTH, 1);|'
plant roll-wave-seed 's|pr_run_seed_offset_rng(\&run->run, run->wave \* 1000 + 512, NULL);|pr_run_seed_offset_rng(\&run->run, run->wave * 500 + 512, NULL);|'
plant common-percent 's|#define CHEST_COMMON 25|#define CHEST_COMMON 24|'
plant ultra-percent 's|#define CHEST_ULTRA 30|#define CHEST_ULTRA 31|'
plant rogue-percent 's|#define CHEST_ROGUE 10|#define CHEST_ROGUE 11|'
plant master-percent 's|#define CHEST_MASTER 5|#define CHEST_MASTER 6|'
plant common-edge-strict 's|if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON) {|if (roll > CHEST_RAND_LENGTH - CHEST_COMMON) {|'
plant ultra-edge-strict 's|} else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA) {|} else if (roll > CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA) {|'
plant rogue-edge-strict 's|} else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE) {|} else if (roll > CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE) {|'
plant master-edge-strict 's|} else if (roll >= CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE - CHEST_MASTER) {|} else if (roll > CHEST_RAND_LENGTH - CHEST_COMMON - CHEST_ULTRA - CHEST_ROGUE - CHEST_MASTER) {|'
plant common-tier-order 's|{MODIFIER_TIER_COMMON, MODIFIER_TIER_COMMON, MODIFIER_TIER_GREAT, MODIFIER_TIER_GREAT};|{MODIFIER_TIER_COMMON, MODIFIER_TIER_GREAT, MODIFIER_TIER_COMMON, MODIFIER_TIER_GREAT};|'
plant common-tier-count 's|tier_shop(run, tiers, 4, "mysteryEncounters/mysteriousChest:option.1.normal");|tier_shop(run, tiers, 3, "mysteryEncounters/mysteriousChest:option.1.normal");|'
plant good-tier 's|{MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA};|{MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ROGUE};|'
plant good-tier-count 's|tier_shop(run, tiers, 3, "mysteryEncounters/mysteriousChest:option.1.good");|tier_shop(run, tiers, 2, "mysteryEncounters/mysteriousChest:option.1.good");|'
plant great-tier 's|{MODIFIER_TIER_ROGUE, MODIFIER_TIER_ROGUE};|{MODIFIER_TIER_ROGUE, MODIFIER_TIER_ULTRA};|'
plant great-tier-count 's|tier_shop(run, tiers, 2, "mysteryEncounters/mysteriousChest:option.1.great");|tier_shop(run, tiers, 1, "mysteryEncounters/mysteriousChest:option.1.great");|'
plant amazing-tier 's|static const int8_t tiers\[1\] = {MODIFIER_TIER_MASTER};|static const int8_t tiers[1] = {MODIFIER_TIER_ROGUE};|'
plant normal-message 's|tier_shop(run, tiers, 4, "mysteryEncounters/mysteriousChest:option.1.normal");|tier_shop(run, tiers, 4, "mysteryEncounters/mysteriousChest:option.1.good");|'
plant amazing-message 's|tier_shop(run, tiers, 1, "mysteryEncounters/mysteriousChest:option.1.amazing");|tier_shop(run, tiers, 1, "mysteryEncounters/mysteriousChest:option.1.great");|'
plant shop-fills 's|    c->fill_remaining = false;|    c->fill_remaining = true;|'
plant shop-heal 's|    gc_me_end_shop(run);|    gc_me_leave_with_heal_shop(run);|'
plant tie-last-wins 's|run->party\[best\].b.level < run->party\[idx\].b.level|run->party[best].b.level <= run->party[idx].b.level|'
plant storage-order 's|        const int idx = run->party_order\[k\];|        const int idx = k;|'
plant fainted-allowed 's|        if (!gc_mon_allowed(\&run->party\[idx\])) continue;|        (void)0;|'
plant ko-hp 's|        ko->b.hp = 0;|        ko->b.hp = 1;|'
plant ko-status 's|        ko->b.status = STATUS_EFFECT_FAINT;|        ko->b.status = 0;|'
plant bad-text-missing 's|        gc_ev_msg(run, "mysteryEncounters/mysteriousChest:option.1.bad", 2, name, pr_arg_key("gimmighoulName", "pokemon:gimmighoul"));|        (void)name;|'
plant fainted-text-missing 's|        gc_ev_msg(run, "battle:fainted", 1, pr_arg_pokemon(\&run->bt, "pokemonNameWithAffix", \&ko->b));|        (void)0;|'
plant game-over-missed 's|        for (int i = 0; i < run->n_party; i++) any \|= gc_mon_allowed(\&run->party\[i\]);|        for (int i = 0; i < run->n_party; i++) any \|= true;|'
plant level-boost 's|floor((double)run->wave / 10 \* 0.5 + 0.5), 0);|floor((double)run->wave / 10 * 1.0 + 0.5), 0);|'
plant level-rounding 's|floor((double)run->wave / 10 \* 0.5 + 0.5), 0);|floor((double)run->wave / 10 * 0.5), 0);|'
plant level-stream 's|    pr_rng r = run->run.rng;|    pr_rng r = gc_me_option_rng(run);|'
plant second-iv-roll 's|SPECIES_ID_GIMMIGHOUL, level, true, boss)|SPECIES_ID_GIMMIGHOUL, level, false, boss)|'
plant rng-not-kept 's|    run->run.rng = r;|    (void)r;|'
plant form-zero 's|        boss->b.form = 0;|        boss->b.form = 1;|'
plant move-nasty-plot 's|gc_mon_set_move(boss, 0, MOVE_ID_NASTY_PLOT);|gc_mon_set_move(boss, 0, MOVE_ID_SHADOW_BALL);|'
plant move-thief 's|gc_mon_set_move(boss, 3, MOVE_ID_THIEF);|gc_mon_set_move(boss, 3, MOVE_ID_TACKLE);|'
plant move-order 's|gc_mon_set_move(boss, 1, MOVE_ID_SHADOW_BALL);|gc_mon_set_move(boss, 1, MOVE_ID_POWER_GEM);|'
plant items-recalc 's|    gc_enemy_finish_party_me(run);|    gc_enemy_finish_party(run);|'
plant reward-forgotten 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant reward-no-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant flee-off 's|    in->me_no_switch = true;|    in->me_no_switch = true; in->me_no_flee = true;|'
plant switch-asked 's|    in->me_no_switch = true;|    in->me_no_switch = false;|'
plant leave-without-heal 's|    if (option != 0) { gc_me_leave_with_heal_shop(run); return; }|    if (option != 0) { gc_me_end_shop(run); return; }|'
plant catch-off 's|    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_TELEPORTING_HIJINKS: case GC_ME_DARK_DEAL: case GC_ME_MYSTERIOUS_CHEST:|    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_TELEPORTING_HIJINKS: case GC_ME_DARK_DEAL:|' gc_mystery.c
plant ask-switch-lost 's|    run->ask_switch = ask_switch;|    (void)ask_switch;|' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
