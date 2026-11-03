#!/bin/sh
# Fiery Fallout: show the comparator (and the host test) can fail. Replays one trace (from tools/fierydiff.sh) through copies of game-core, each with one line of src/gc_fiery.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_fiery_diff_test.c exiting 1, or, for what the oracle does not play (the battle: switching prompt, the maximum stack of the lead's booster), by tests/mystery_fiery_test.c failing.
# Not planted: a Pokemon's unlocked passive (the oracle's party has none), the post-summon handler in battle-core (its stages are compared through the Volcarona's stat stages), the sprites and the lava animations (the app's).
# Usage: tools/fieryplant.sh <trace>   (FIERY_PLANT_FROM=<name> starts at that plant, FIERY_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-fiery-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_fiery.c}
  [ "$NAME" != "${FIERY_PLANT_FROM:-}" ] || started=1
  [ -z "${FIERY_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${FIERY_PLANT_ONLY:-}" ] || case " $FIERY_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_fiery_diff_test" "$W/build/mystery_fiery_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_fiery_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_fiery_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

plant weather-not-set 's|    bt->weather = WEATHER_TYPE_SUNNY;|    (void)bt;|'
plant weather-turns 's|    bt->weather_turns_left = bt->weather_max_duration = 0;|    bt->weather_turns_left = bt->weather_max_duration = 5;|'
plant weather-primal-overwritten 's|    if (pr_weather_immutable(bt->weather)) {|    if (0) {|'
plant weather-same-reset 's|    if (bt->weather == WEATHER_TYPE_SUNNY) return;|    (void)bt;|'
plant weather-message-lost 's|    gc_ev_msg(run, pr_weather_key(WEATHER_TYPE_SUNNY, PR_WEATHER_START), 0);|    (void)run;|'
plant weather-continue-lost 's|        gc_ev_msg(run, pr_weather_key(bt->weather, PR_WEATHER_CONTINUE), 0);|        (void)run;|'
plant init-not-called 's|        gc_fiery_init(run);|        (void)run;|' gc_mystery.c
plant ability-list-thick-fat 's|ABILITY_ID_THICK_FAT, ||'
plant ability-list-water-veil 's|ABILITY_ID_WATER_VEIL,|ABILITY_ID_NONE,|'
# (ability-ignores-override is an equivalent mutant: ignoreOverride only skips a Skill Swap / Gastro Acid ability in the summon data, which a party Pokemon outside a battle does not have)
plant fire-fainted-ok 's|        if (m->b.hp > 0 \&\& is_fire(m)) return true;|        if (is_fire(m)) return true;|'
plant ability-fainted-ok 's|        if (gc_mon_allowed(m) \&\& fire_resistant_ability(m)) return true;|        if (fire_resistant_ability(m)) return true;|'
plant primary-fire-unfainted 's|        if (is_fire(\&run->party\[run->party_order\[k\]\])) return run->party_order\[k\];|        if (is_fire(\&run->party[run->party_order[k]]) \&\& run->party[run->party_order[k]].b.hp > 0) return run->party_order[k];|'
plant primary-storage-order 's|        if (is_fire(\&run->party\[run->party_order\[k\]\])) return run->party_order\[k\];|        if (is_fire(\&run->party[k])) return k;|'
plant primary-ability-fainted 's|        if (gc_mon_allowed(\&run->party\[run->party_order\[k\]\]) \&\& fire_resistant_ability(\&run->party\[run->party_order\[k\]\])) return run->party_order\[k\];|        if (fire_resistant_ability(\&run->party[run->party_order[k]])) return run->party_order[k];|'
plant calm-always-on 's|        s->option_ok\[2\] = gc_fiery_option_ok(run);|        s->option_ok[2] = true;|' gc_mystery.c
plant token-volcarona 's|pr_arg_key("volcaronaName", "pokemon:volcarona")|pr_arg_key("volcaronaName", "pokemon:charizard")|' gc_mystery.c
plant token-primary-name '395s|"option3PrimaryName"|"option2PrimaryName"|' gc_mystery.c
plant catch-off 's|    case GC_ME_BERRIES_ABOUND: case GC_ME_SLUMBERING_SNORLAX: case GC_ME_FIERY_FALLOUT: case GC_ME_UNCOMMON_BREED:|    case GC_ME_BERRIES_ABOUND: case GC_ME_SLUMBERING_SNORLAX: case GC_ME_UNCOMMON_BREED:|' gc_mystery.c
plant lead-item-stream 's|    run->run.rng = pr_run_seed_offset_rng(\&run->run, run->wave \* 1000, NULL);|    (void)saved;|'
plant lead-item-offset 's|run->wave \* 1000, NULL);|run->wave * 1000 + 512, NULL);|'
plant lead-item-fallback 's|    if (type < 0) type = POKEMON_TYPE_FIRE;.*|    if (type < 0) type = POKEMON_TYPE_NORMAL;|'
plant lead-storage-order 's|    gc_mon \*lead = \&run->party\[run->party_order\[0\]\];|    gc_mon *lead = \&run->party[0];|'
plant lead-max-stack 's|    if (held < 99) gc_item_add|    if (held < 100) gc_item_add|'
plant lead-held-ignored 's|it->arg == type \&\& it->pokemon_id == lead->b.id) held += it->stack;|it->arg == type \&\& it->pokemon_id == 0) held += it->stack;|'
plant lead-text-lost 's|fieryFallout:foundItem"|fieryFallout:found"|'
plant fight-one-level-roll 's|    for (int i = 0; i < 2; i++) level\[i\] = pr_run_get_level_for_wave(\&run->run, \&r, run->wave);|    level[0] = level[1] = pr_run_get_level_for_wave(\&run->run, \&r, run->wave);|'
plant fight-stream '/^static void fight/,/^}/ s|pr_rng r = gc_me_option_rng(run);|pr_rng r = run->run.rng;|'
plant fight-boss 's|SPECIES_ID_VOLCARONA, level\[i\], false, false, \&made\[i\])|SPECIES_ID_VOLCARONA, level[i], true, false, \&made[i])|'
plant fight-second-iv 's|SPECIES_ID_VOLCARONA, level\[i\], false, false, \&made\[i\])|SPECIES_ID_VOLCARONA, level[i], false, true, \&made[i])|'
plant gender-swapped 's|e->b.gender = i ? GENDER_FEMALE : GENDER_MALE;|e->b.gender = i ? GENDER_MALE : GENDER_FEMALE;|'
plant gender-not-set 's|        e->b.gender = i ? GENDER_FEMALE : GENDER_MALE;|        (void)i;|'
plant post-summon-none 's|        e->b.me_post_summon = PR_ME_POST_SPDEF_SPD;|        e->b.me_post_summon = PR_ME_POST_NONE;|'
plant one-enemy 's|    run->n_enemy = 2;|    run->n_enemy = 1;|'
plant double-off 's|    run->double_battle = true;|    run->double_battle = false;|'
plant start-effect-target 's|    gc_me_add_start_effect(run, 3, 1, MOVE_ID_FIRE_SPIN);|    gc_me_add_start_effect(run, 3, 0, MOVE_ID_FIRE_SPIN);|'
plant start-effect-source 's|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_FIRE_SPIN);|    gc_me_add_start_effect(run, 3, 0, MOVE_ID_FIRE_SPIN);|'
plant start-effect-move 's|    gc_me_add_start_effect(run, 3, 1, MOVE_ID_FIRE_SPIN);|    gc_me_add_start_effect(run, 3, 1, MOVE_ID_EMBER);|'
plant start-effect-lost 's|    gc_me_add_start_effect(run, 3, 1, MOVE_ID_FIRE_SPIN);|    (void)run;|'
plant flee-allowed 's|    in->me_no_flee = true;|    in->me_no_flee = false;|'
plant switch-allowed 's|    in->me_no_switch = true;|    in->me_no_switch = false;|'
plant reward-active 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant reward-no-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant reward-callback-off 's|    in->me_reward.before = true;|    in->me_reward.before = false;|'
plant lead-item-pending-off 's|    in->fiery_item = true;|    in->fiery_item = false;|'
plant damage-percent 's|#define DAMAGE_PERCENTAGE 20|#define DAMAGE_PERCENTAGE 25|'
plant damage-fire-too 's|        if (gc_mon_allowed(\&run->party\[idx\]) \&\& !is_fire(\&run->party\[idx\])) pool\[n++\] = idx;|        if (gc_mon_allowed(\&run->party[idx])) pool[n++] = idx;|'
# (damage-fainted-too is an equivalent mutant: the damage helper leaves a Pokemon at 0 HP as it is and a fainted one is no burn candidate)
plant damage-faints 's|    if (m->b.hp > 0 \&\& m->b.hp - damage <= 0) damage = m->b.hp - 1;|    (void)damage;|' gc_mystery.c
plant hunker-stream '/^static void hunker/,/^}/ s|pr_rng r = gc_me_option_rng(run);|pr_rng r = run->run.rng;|'
plant burn-roll-min 's|burnable\[pr_rng_seed_int(\&r, nb, 0)\]|burnable[nb - 1 - pr_rng_seed_int(\&r, nb, 0)]|'
plant burn-pool-all 's|status == STATUS_EFFECT_NONE) burnable|status >= STATUS_EFFECT_NONE) burnable|'
plant burn-no-check 's|        if (pr_can_set_status(\&run->bt, \&run->eng, \&m->b, STATUS_EFFECT_BURN, true, false, NULL, false)) {|        if (1) {|'
plant burn-status 's|            m->b.status = STATUS_EFFECT_BURN;|            m->b.status = STATUS_EFFECT_POISON;|'
plant heatproof-missing 's|            else m->b.custom_ability = ABILITY_ID_HEATPROOF;|            else (void)m;|'
plant heatproof-wrong 's|            else m->b.custom_ability = ABILITY_ID_HEATPROOF;|            else m->b.custom_ability = ABILITY_ID_FLASH_FIRE;|'
plant burn-text-lost 's|"mysteryEncounters/fieryFallout:option.2.targetBurned"|"mysteryEncounters/fieryFallout:option.2.targetBurnX"|'
plant hunker-no-heal-shop '/^static void hunker/,/^}/ s|    gc_me_leave_with_heal_shop(run);|    gc_me_leave(run);|'
plant calm-exp-amount 's|base_exp \* 2, true);|base_exp, true);|'
plant calm-exp-wave 's|base_exp \* 2, true);|base_exp * 2, false);|'
plant calm-exp-lost 's|    if (primary >= 0) gc_me_exp_value|    if (0) gc_me_exp_value|'
plant calm-no-fill 's|    c->fill_remaining = true;|    c->fill_remaining = false;|'
plant calm-item-lost '/^static void calm/,/^}/ s|    give_lead_item(run);|    (void)run;|'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
