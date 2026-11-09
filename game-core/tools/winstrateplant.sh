#!/bin/sh
# The Winstrate Challenge (src/gc_winstrate.c): show the comparator (and the host test) can fail. Replays one trace (from tools/winstratediff.sh) through copies of game-core, each with one line of src/gc_winstrate.c (or gc_phases.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_winstrate_diff_test.c exiting 1, or, for what the oracle does not play (the battles between the fights: the weather, the prize money, the hook that starts the next fight), by tests/mystery_winstrate_test.c failing.
# Not planted (equivalent for this encounter): the member's base_nature (the comparator reads the nature the Pokemon fights with, which the same function sets), the levels call's is_double flag (read only for a one-member double template; every team here is single), the member context's shiny_lock (a trainer's member is shiny only when (trainerId ^ secretId) ^ (id half ^ other half) is below the base chance, about 1 in 16000: the oracle's random ids never meet it).
# Not planted either (unobservable here): the party template's prev_wave argument (read only by templates that depend on the previous wave), the explicit gc_arena_reset_effects and gc_battle_cleanup_players calls of gc_winstrate_next (gc_battle_setup_field's own outgoing-field cleanup and the arena it keeps do the same: they are kept for upstream's order), the `winstrate.active` guard of gc_winstrate_next.
# Usage: tools/winstrateplant.sh <trace>   (WINSTRATE_PLANT_FROM=<name> starts at that plant, WINSTRATE_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-winstrate-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_winstrate.c}
  [ "$NAME" != "${WINSTRATE_PLANT_FROM:-}" ] || started=1
  [ -z "${WINSTRATE_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${WINSTRATE_PLANT_ONLY:-}" ] || case " $WINSTRATE_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  case "$FILE" in
    battle-core/*) rm "$W/battle-core"; (cd "$ROOT" && tar cf - --exclude=./battle-core/build battle-core) | tar xf - -C "$W"; TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *) TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_winstrate_diff_test" "$W/build/mystery_winstrate_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_winstrate_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_winstrate_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}


TAB='/^static const w_team TEAMS/,/^};/'
MEMBER='/^static bool make_member/,/^}/'
BUILD_F='/^static bool build_fight/,/^}/'
plant t-victor 's#{TRAINER_TYPE_VICTOR, 2, {#{TRAINER_TYPE_VICTORIA, 2, {#'
plant t-victoria 's#{TRAINER_TYPE_VICTORIA, 2, {#{TRAINER_TYPE_VICTOR, 2, {#'
plant t-vivi-size 's#{TRAINER_TYPE_VIVI, 3, {#{TRAINER_TYPE_VIVI, 2, {#'
plant t-vicky 's#{TRAINER_TYPE_VICKY, 1, {#{TRAINER_TYPE_VIVI, 1, {#'
plant t-vito 's#{TRAINER_TYPE_VITO, 5, {#{TRAINER_TYPE_VICKY, 5, {#'
plant swellow-ability 's#{SPECIES_ID_SWELLOW, 0, 0, NATURE_ADAMANT#{SPECIES_ID_SWELLOW, 0, 1, NATURE_ADAMANT#'
plant swellow-nature 's#{SPECIES_ID_SWELLOW, 0, 0, NATURE_ADAMANT#{SPECIES_ID_SWELLOW, 0, 0, NATURE_JOLLY#'
plant swellow-moves 's#MOVE_ID_BRAVE_BIRD, MOVE_ID_PROTECT#MOVE_ID_PROTECT, MOVE_ID_BRAVE_BIRD#'
plant swellow-band 's#LOCKED("FOCUS_BAND", -1, 2)#LOCKED("FOCUS_BAND", -1, 1)#'
plant swellow-lock 's#{LOCKED("FLAME_ORB", -1, 1), LOCKED("FOCUS_BAND"#{FREE("FLAME_ORB", -1, 1), LOCKED("FOCUS_BAND"#'
plant obstagoon-ability 's#{SPECIES_ID_OBSTAGOON, 0, 1, NATURE_ADAMANT#{SPECIES_ID_OBSTAGOON, 0, 0, NATURE_ADAMANT#'
plant obstagoon-leftovers 's#LOCKED("LEFTOVERS", -1, 2)#LOCKED("LEFTOVERS", -1, 1)#'
plant obstagoon-moves 's#MOVE_ID_NIGHT_SLASH, MOVE_ID_FIRE_PUNCH#MOVE_ID_FIRE_PUNCH, MOVE_ID_NIGHT_SLASH#'
plant roserade-ability 's#{SPECIES_ID_ROSERADE, 0, 0, NATURE_CALM#{SPECIES_ID_ROSERADE, 0, 1, NATURE_CALM#'
plant roserade-nature 's#{SPECIES_ID_ROSERADE, 0, 0, NATURE_CALM#{SPECIES_ID_ROSERADE, 0, 0, NATURE_BOLD#'
plant roserade-claw 's#{LOCKED("SOUL_DEW", -1, 1), LOCKED("QUICK_CLAW", -1, 2)#{LOCKED("SOUL_DEW", -1, 1), LOCKED("QUICK_CLAW", -1, 1)#'
plant roserade-dew 's#LOCKED("SOUL_DEW", -1, 1)#LOCKED("SOUL_DEW", -1, 2)#'
plant gardevoir-form 's#{SPECIES_ID_GARDEVOIR, 1, -1, NATURE_TIMID#{SPECIES_ID_GARDEVOIR, 0, -1, NATURE_TIMID#'
plant gardevoir-ability 's#{SPECIES_ID_GARDEVOIR, 1, -1, NATURE_TIMID#{SPECIES_ID_GARDEVOIR, 1, 0, NATURE_TIMID#'
plant gardevoir-nature 's#{SPECIES_ID_GARDEVOIR, 1, -1, NATURE_TIMID#{SPECIES_ID_GARDEVOIR, 1, -1, NATURE_MODEST#'
plant gardevoir-booster 's#LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_PSYCHIC, 1)#LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_PSYCHIC, 2)#'
plant gardevoir-type 's#LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FAIRY, 1)#LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_GHOST, 1)#'
plant seaking-ability 's#{SPECIES_ID_SEAKING, 0, 3, NATURE_ADAMANT#{SPECIES_ID_SEAKING, 0, 1, NATURE_ADAMANT#'
plant seaking-berry 's#LOCKED("BERRY", BERRY_TYPE_LUM, 2), LOCKED("BASE_STAT_BOOSTER", STAT_HP, 4)#LOCKED("BERRY", BERRY_TYPE_SITRUS, 2), LOCKED("BASE_STAT_BOOSTER", STAT_HP, 4)#'
plant seaking-stat 's#LOCKED("BERRY", BERRY_TYPE_LUM, 2), LOCKED("BASE_STAT_BOOSTER", STAT_HP, 4)#LOCKED("BERRY", BERRY_TYPE_LUM, 2), LOCKED("BASE_STAT_BOOSTER", STAT_ATK, 4)#'
plant breloom-ability 's#{SPECIES_ID_BRELOOM, 0, 1, NATURE_JOLLY#{SPECIES_ID_BRELOOM, 0, 0, NATURE_JOLLY#'
plant breloom-orb 's#LOCKED("TOXIC_ORB", -1, 1)#FREE("TOXIC_ORB", -1, 1)#'
plant breloom-moves 's#MOVE_ID_SEED_BOMB, MOVE_ID_DRAIN_PUNCH#MOVE_ID_DRAIN_PUNCH, MOVE_ID_SEED_BOMB#'
plant camerupt-form 's#{SPECIES_ID_CAMERUPT, 1, -1, NATURE_CALM#{SPECIES_ID_CAMERUPT, 0, -1, NATURE_CALM#'
plant camerupt-claw 's#LOCKED("QUICK_CLAW", -1, 3)#LOCKED("QUICK_CLAW", -1, 2)#'
plant camerupt-nature 's#{SPECIES_ID_CAMERUPT, 1, -1, NATURE_CALM#{SPECIES_ID_CAMERUPT, 1, -1, NATURE_BOLD#'
plant medicham-form 's#{SPECIES_ID_MEDICHAM, 1, -1, NATURE_IMPISH#{SPECIES_ID_MEDICHAM, 0, -1, NATURE_IMPISH#'
plant medicham-bell 's#LOCKED("SHELL_BELL", -1, 1)#FREE("SHELL_BELL", -1, 1)#'
plant medicham-moves 's#MOVE_ID_ZEN_HEADBUTT, MOVE_ID_BULLET_PUNCH#MOVE_ID_BULLET_PUNCH, MOVE_ID_ZEN_HEADBUTT#'
plant electrode-species 's#{SPECIES_ID_HISUI_ELECTRODE, 0, 0, NATURE_MODEST#{SPECIES_ID_ELECTRODE, 0, 0, NATURE_MODEST#'
plant electrode-ability 's#{SPECIES_ID_HISUI_ELECTRODE, 0, 0, NATURE_MODEST#{SPECIES_ID_HISUI_ELECTRODE, 0, 1, NATURE_MODEST#'
plant electrode-speed 's#LOCKED("BASE_STAT_BOOSTER", STAT_SPD, 2)#LOCKED("BASE_STAT_BOOSTER", STAT_SPD, 1)#'
plant swalot-ability 's#{SPECIES_ID_SWALOT, 0, 2, NATURE_QUIET#{SPECIES_ID_SWALOT, 0, 1, NATURE_QUIET#'
plant swalot-count 's#MOVE_ID_ICE_BEAM, MOVE_ID_EARTHQUAKE}, 11,#MOVE_ID_ICE_BEAM, MOVE_ID_EARTHQUAKE}, 10,#'
plant swalot-berry 's#FREE("BERRY", BERRY_TYPE_APICOT, 2)#FREE("BERRY", BERRY_TYPE_APICOT, 1)#'
plant swalot-berry-type 's#FREE("BERRY", BERRY_TYPE_LEPPA, 2)#FREE("BERRY", BERRY_TYPE_LANSAT, 2)#'
plant swalot-lock 's#FREE("BERRY", BERRY_TYPE_ENIGMA, 2)#LOCKED("BERRY", BERRY_TYPE_ENIGMA, 2)#'
plant swalot-nature 's#{SPECIES_ID_SWALOT, 0, 2, NATURE_QUIET#{SPECIES_ID_SWALOT, 0, 2, NATURE_RELAXED#'
plant dodrio-ability 's#{SPECIES_ID_DODRIO, 0, 2, NATURE_JOLLY#{SPECIES_ID_DODRIO, 0, 0, NATURE_JOLLY#'
plant dodrio-rock 's#LOCKED("KINGS_ROCK", -1, 2)#LOCKED("KINGS_ROCK", -1, 1)#'
plant dodrio-moves 's#MOVE_ID_THRASH, MOVE_ID_KNOCK_OFF#MOVE_ID_KNOCK_OFF, MOVE_ID_THRASH#'
plant alakazam-form 's#{SPECIES_ID_ALAKAZAM, 1, -1, NATURE_BOLD#{SPECIES_ID_ALAKAZAM, 0, -1, NATURE_BOLD#'
plant alakazam-lens 's#LOCKED("WIDE_LENS", -1, 2)#LOCKED("WIDE_LENS", -1, 1)#'
plant alakazam-nature 's#{SPECIES_ID_ALAKAZAM, 1, -1, NATURE_BOLD#{SPECIES_ID_ALAKAZAM, 1, -1, NATURE_TIMID#'
plant darmanitan-ability 's#{SPECIES_ID_DARMANITAN, 0, 0, NATURE_IMPISH#{SPECIES_ID_DARMANITAN, 0, 1, NATURE_IMPISH#'
plant darmanitan-claw 's#{LOCKED("QUICK_CLAW", -1, 2)}}}},#{LOCKED("QUICK_CLAW", -1, 1)}}}},#'
plant darmanitan-moves 's#MOVE_ID_FLARE_BLITZ, MOVE_ID_ROCK_SLIDE#MOVE_ID_ROCK_SLIDE, MOVE_ID_FLARE_BLITZ#'
plant member-slot "$MEMBER"'s#ec.trainer_slot = TRAINER_SLOT_TRAINER;#ec.trainer_slot = TRAINER_SLOT_NONE;#'
plant member-boss "$MEMBER"'s#ec.boss = false;#ec.boss = true;#'
plant member-moves "$MEMBER"'s#const bool made = gc_generate_moveset(run, &run->run.rng, out);#const bool made = false;#'
plant member-g-nature "$MEMBER"'s#g.nature = w->nature;#g.nature = g.nature;#'
plant member-form "$MEMBER"'s#g.form = (uint8_t)w->form;#g.form = 0;#'
plant member-ability "$MEMBER"'s#if (w->ability >= 0) g.ability_index#if (0 \&\& w->ability >= 0) g.ability_index#'
plant member-set-moves "$MEMBER"'s#gc_mon_set_move(out, k, w->moves\[k\]);#gc_mon_set_move(out, k, w->moves[0]);#'
plant fight-female "$BUILD_F"'s#pr_rng_seed_int(&run->run.rng, 2, 0) != 0;#pr_rng_seed_int(\&run->run.rng, 2, 0) == 0;#'
plant fight-no-female-roll "$BUILD_F"'s#const bool female = pr_rng_seed_int(&run->run.rng, 2, 0) != 0;#const bool female = false;#'
plant fight-double "$BUILD_F"'s#^    run->double_battle = false;#    run->double_battle = true;#'
plant fight-holder "$BUILD_F"'s#items\[n\].pokemon_id = run->enemy\[e\].b.id;#items[n].pokemon_id = run->enemy[0].b.id;#'
plant fight-lock "$BUILD_F"'s#items\[n\].non_transferable = t->mon\[e\].items\[i\].locked;#items[n].non_transferable = false;#'
plant fight-arg "$BUILD_F"'s#items\[n\].arg = t->mon\[e\].items\[i\].arg;#items[n].arg = -1;#'
plant fight-finish "$BUILD_F"'s#gc_enemy_finish_party_holders(run, items, n);#gc_enemy_finish_party_me(run);#'
plant fight-me-battle "$BUILD_F"'s#run->me_battle = true;#run->me_battle = false;#'
plant fight-catch "$BUILD_F"'s#run->me_catch_allowed = false;#run->me_catch_allowed = true;#'
plant fight-nostats "$BUILD_F"'s#in->me_no_stats = false;#in->me_no_stats = true;#'
plant fight-reward "$BUILD_F"'s#in->me_reward.active = false;#in->me_reward.active = true;#'
plant fight-name "$BUILD_F"'s#in->custom_trainer_name\[0\] = 0;#snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "x");#'
plant say-dialogue 's#^    gc_ev_trainer_dialogue(run, 0);.*#    (void)0;#'
plant say-trainer 's#^    if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }#    if (ev) { ev->a = run->trainer_type; ev->b = 0; }#'
plant finish-voucher 's#gc_reward_gain(run, "VOUCHER_PREMIUM");#gc_reward_gain(run, "VOUCHER_PLUS");#'
plant finish-victory 's#gc_ev_msg(run, NS ":victory2", 0);#gc_ev_msg(run, NS ":victory", 0);#'
plant finish-victory-first 's#gc_ev_msg(run, NS ":victory", 0);#gc_ev_msg(run, NS ":victory2", 0);#'
plant finish-tier 's#.tier = MODIFIER_TIER_MASTER;#.tier = MODIFIER_TIER_ROGUE;#'
plant finish-item 's#PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE, -1);#PR_ITEM_RARER_CANDY, -1);#'
plant finish-fill 's#^    c->fill_remaining = false;#    c->fill_remaining = true;#'
plant finish-active '/^static void finish/,/^}/s#^    in->winstrate.active = false;#    in->winstrate.active = true;#'
plant finish-shop 's#^    gc_me_end_shop(run);#    (void)0;#'
plant exec-option 's#if (option != 0) {#if (option != 1) {#'
plant exec-heal 's#gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));#(void)0;#'
plant exec-candy 's#gc_me_leave_with_shop(run, "RARER_CANDY");#gc_me_leave_with_shop(run, "RARE_CANDY");#'
plant exec-active 's#^    in->winstrate.active = true;#    in->winstrate.active = false;#'
plant exec-next 's#^    in->winstrate.next = 1;#    in->winstrate.next = 0;#'
plant next-end 's#if (k >= GC_WINSTRATE_FIGHTS) { finish(run); return; }#if (k >= GC_WINSTRATE_FIGHTS - 1) { finish(run); return; }#'
plant next-counter 's#in->winstrate.next = (uint8_t)(k + 1);#in->winstrate.next = (uint8_t)k;#'
plant victory-trainer 's#^    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));#    (void)0;#'
plant victory-end 's#^    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));#    gc_rq_push(run, gc_rp(RP_BATTLE_END, 0, 0, 0));#'
plant victory-next 's#^    gc_rq_push(run, gc_rp(RP_WINSTRATE, 0, 0, 0));#    (void)0;#'
plant hook-victory 's#if (in->winstrate.active) {   /\* The Winstrate#if (0 \&\& in->winstrate.active) {   /* The Winstrate#' gc_phases.c
plant hook-phase 's#^        gc_winstrate_next(run);#        (void)0;#' gc_phases.c
plant stand-in 's#^        gc_winstrate_execute(run, option);#        end_mystery_encounter_leave(run);#' gc_mystery.c

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
