#!/bin/sh
# A Trainer's Test (src/gc_trtest.c): show the comparator (and the host test) can fail. Replays one trace (from tools/trtestdiff.sh) through copies of game-core, each with one line of src/gc_trtest.c (or gc_phases.c / gc_mystery.c / gc_mon.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_trtest_diff_test.c exiting 1, or, for what the oracle does not play (the win's phases, the shop, the loss), by tests/mystery_trtest_test.c failing.
# Not planted (equivalent for this encounter): the party template's trainer (the Elite Four template is shared by Lorelei, Bruno, Agatha, Lance and every stat trainer: the same sizes and strengths), the levels call's is_double flag (read only for a one-member double template; this team is a single six).
# Not planted either (unobservable here): BattleEndPhase's victory flag (the shop is already queued, so the loss branch that would queue one is skipped, and the battle score it adds is 0 because two bosses squeeze the score multiplier to an ease of 0), gc_enemy_finish_party_me against gc_enemy_finish_party (the plant changed nothing the comparator or host test reads, over 150 fights with stat boosters held; why was not traced).
# Usage: tools/trtestplant.sh <trace>   (TRTEST_PLANT_FROM=<name> starts at that plant, TRTEST_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-trtest-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_trtest.c}
  [ "$NAME" != "${TRTEST_PLANT_FROM:-}" ] || started=1
  [ -z "${TRTEST_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${TRTEST_PLANT_ONLY:-}" ] || case " $TRTEST_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
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
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_trtest_diff_test" "$W/build/mystery_trtest_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_trtest_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_trtest_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

BUILD_F='/^static bool build_fight/,/^}/'
EXEC='/^void gc_trtest_execute/,/^}/'
VICT='/^void gc_trtest_victory/,/^}/'
REW='/^void gc_trtest_reward/,/^}/'
OUTRO='/^void gc_trtest_outro/,/^}/'
plant t-buck 's#{TRAINER_TYPE_BUCK, "buck"}#{TRAINER_TYPE_CHERYL, "buck"}#'
plant t-cheryl 's#{TRAINER_TYPE_CHERYL, "cheryl"}#{TRAINER_TYPE_MARLEY, "cheryl"}#'
plant t-marley 's#{TRAINER_TYPE_MARLEY, "marley"}#{TRAINER_TYPE_MIRA, "marley"}#'
plant t-mira 's#{TRAINER_TYPE_MIRA, "mira"}#{TRAINER_TYPE_RILEY, "mira"}#'
plant t-riley 's#{TRAINER_TYPE_RILEY, "riley"}#{TRAINER_TYPE_BUCK, "riley"}#'
plant k-buck 's#{TRAINER_TYPE_BUCK, "buck"}#{TRAINER_TYPE_BUCK, "cheryl"}#'
plant k-cheryl 's#{TRAINER_TYPE_CHERYL, "cheryl"}#{TRAINER_TYPE_CHERYL, "marley"}#'
plant k-marley 's#{TRAINER_TYPE_MARLEY, "marley"}#{TRAINER_TYPE_MARLEY, "mira"}#'
plant k-mira 's#{TRAINER_TYPE_MIRA, "mira"}#{TRAINER_TYPE_MIRA, "riley"}#'
plant k-riley 's#{TRAINER_TYPE_RILEY, "riley"}#{TRAINER_TYPE_RILEY, "buck"}#'
plant pick-count 's#return pr_rng_seed_int(&r, 5, 0);#return pr_rng_seed_int(\&r, 4, 0);#'
plant pick-offset 's#pr_run_seed_offset_rng(&run->run, run->wave, NULL)#pr_run_seed_offset_rng(\&run->run, run->wave + 1, NULL)#'
plant fight-female "$BUILD_F"'s#pr_rng_seed_int(&run->run.rng, 2, 0) != 0;#pr_rng_seed_int(\&run->run.rng, 2, 0) == 0;#'
plant fight-no-female-roll "$BUILD_F"'s#const bool female = pr_rng_seed_int(&run->run.rng, 2, 0) != 0;#const bool female = false;#'
plant fight-type "$BUILD_F"'s#^    run->trainer_type = t;#    run->trainer_type = TRAINER_TYPE_BUCK;#'
plant fight-battle-type "$BUILD_F"'s#^    run->battle_type = BATTLE_TYPE_TRAINER;#    run->battle_type = BATTLE_TYPE_WILD;#'
plant fight-double "$BUILD_F"'s#^    run->double_battle = false;#    run->double_battle = true;#'
plant fight-add "$BUILD_F"'s#run->wave / 10.0 \* 1.0#run->wave / 10.0 * 0.0#'
plant fight-add-div "$BUILD_F"'s#run->wave / 10.0 \* 1.0#run->wave / 20.0 * 1.0#'
plant fight-add-all "$BUILD_F"'s#^    for (int i = 0; i < n; i++) levels\[i\] += add;#    for (int i = 1; i < n; i++) levels[i] += add;#'
plant fight-variant "$BUILD_F"'s#female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT#female ? TRAINER_VARIANT_DEFAULT : TRAINER_VARIANT_FEMALE#'
plant fight-name "$BUILD_F"'s#^    in->custom_trainer_name\[0\] = 0;#    snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "x");#'
plant exec-option "$EXEC"'s#if (option != 0) {#if (option != 1) {#'
plant exec-rare-post "$EXEC"'s#^        in->me_post.c = EGG_RARE;#        in->me_post.c = EGG_EPIC;#'
plant exec-rare-egg "$EXEC"'s#^        in->trtest.egg = EGG_RARE;#        in->trtest.egg = EGG_EPIC;#'
plant exec-epic-post "$EXEC"'s#^    in->me_post.c = EGG_EPIC;#    in->me_post.c = EGG_RARE;#'
plant exec-epic-egg "$EXEC"'s#^    in->trtest.egg = EGG_EPIC;#    in->trtest.egg = EGG_RARE;#'
plant exec-heal "$EXEC"'s#^        gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));#        (void)0;#'
plant exec-egg-refuse "$EXEC"'s#^        gc_trtest_reward(run);.*#        (void)0;#'
plant exec-shop '0,/^        gc_me_leave_with_heal_shop(run);/s#^        gc_me_leave_with_heal_shop(run);#        gc_me_leave_with_shop(run, "RARER_CANDY");#'
plant exec-post "$EXEC"'s#^    in->me_post.pending = true;.*#    in->me_post.pending = false;#'
plant exec-post-pick "$EXEC"'s#^    in->me_post.b = pick;#    in->me_post.b = 0;#'
plant exec-me-battle "$EXEC"'s#^    run->me_battle = true;#    run->me_battle = false;#'
plant exec-catch "$EXEC"'s#run->me_catch_allowed = false;#run->me_catch_allowed = true;#'
plant exec-reward "$EXEC"'s#^    in->me_reward.active = false;#    in->me_reward.active = true;#'
plant exec-nostats "$EXEC"'s#^    in->me_no_stats = false;#    in->me_no_stats = true;#'
plant exec-active "$EXEC"'s#^    in->trtest.active = true;#    in->trtest.active = false;#'
plant exec-say "$EXEC"'s#^    gc_me_say_trainer_fight(run, in);#    (void)0;#'
plant victory-trainer "$VICT"'s#^    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));#    (void)0;#'
plant victory-rewards "$VICT"'s#^    gc_rq_push(run, gc_rp(RP_ME_BEFORE_REWARDS, 0, 0, 0));#    (void)0;#'
plant victory-item "$VICT"'s#gc_reward_item_index("RELIC_GOLD")#gc_reward_item_index("LEFTOVERS")#'
plant victory-tier1 '0,/^    c->tiers\[c->n_tiers++\] = MODIFIER_TIER_ROGUE;/s//    c->tiers[c->n_tiers++] = MODIFIER_TIER_ULTRA;/'
plant victory-tier2 '/^    c->tiers\[c->n_tiers++\] = MODIFIER_TIER_ROGUE;/{n;s/MODIFIER_TIER_ROGUE/MODIFIER_TIER_ULTRA/}'
plant victory-fill "$VICT"'s#c->fill_remaining = true;#c->fill_remaining = false;#'
plant victory-shop "$VICT"'s#^    gc_me_end_with_shop(run);#    (void)0;#'
plant victory-active "$VICT"'s#^    in->trtest.active = false;#    in->trtest.active = true;#'
plant reward-once "$REW"'s#^    in->trtest.egg = 0;#    (void)0;#'
plant reward-source "$REW"'s#gc_system_gift_egg_source(run->sys, EGG_SOURCE_EVENT, tier, seed, 0)#gc_system_gift_egg_source(run->sys, 0, tier, seed, 0)#'
plant reward-tier "$REW"'s#gc_system_gift_egg_source(run->sys, EGG_SOURCE_EVENT, tier, seed, 0)#gc_system_gift_egg_source(run->sys, EGG_SOURCE_EVENT, tier == 1 ? 2 : 1, seed, 0)#'
plant outro-epic 's#p->c == EGG_EPIC ? "mysteryEncounters/aTrainersTest:eggTypes.epic" : "mysteryEncounters/aTrainersTest:eggTypes.rare"#p->c == EGG_EPIC ? "mysteryEncounters/aTrainersTest:eggTypes.rare" : "mysteryEncounters/aTrainersTest:eggTypes.epic"#'
plant outro-name 's#snprintf(name, sizeof name, "trainerNames:%s", gc_trtest_key(p->b));#snprintf(name, sizeof name, "trainerNames:%s", gc_trtest_key(0));#'
plant hook-victory 's#^        gc_trtest_victory(run);#        (void)0;#' gc_phases.c
plant hook-reward 's#gc_trtest_reward(run); }#}#' gc_phases.c
plant stand-in 's#^        gc_trtest_execute(run, option);#        end_mystery_encounter_leave(run);#' gc_mystery.c
plant pokeball 's#m->pokeball = (uint8_t)g->pokeball;#m->pokeball = 0;#' gc_mon.c

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
