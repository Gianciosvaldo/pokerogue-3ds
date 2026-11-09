#!/bin/sh
# The Bug-Type Superfan's fight (option 1): show the comparator (and the host test) can fail. Replays one trace (from tools/superfanfightdiff.sh) through copies of game-core, each with one line of src/gc_superfan.c (or gc_encounter.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_superfan_fight_diff_test.c exiting 1, or, for what the oracle does not play (the events of the opening), by tests/mystery_superfan_test.c failing.
# Not planted (equivalent for this fight): gc_enemy_finish_party against gc_enemy_finish_party_me (the oracle's stats come out the same either way: tools/superfanfightdiff.sh compares every stat), the item-pool thresholds helper (only the given-items path calls it, this fight rolls its items). the member context's shiny_lock (a trainer's member is shiny only when (trainerId ^ secretId) ^ (id half ^ other half) is below the base chance, about 1 in 16000: the oracle's random ids never meet it, and the rest of the path is the same). The member's fusion_species (-1 or 0: the base generator sets it itself), the levels call's is_double flag and the template's default strength (both read only for a one-member double template; the team has 3 to 5) and the template's offsets (unused by the level formula).
# Usage: tools/superfanfightplant.sh <trace>   (SUPERFAN_PLANT_FROM=<name> starts at that plant, SUPERFAN_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-superfan-fight-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_superfan.c}
  [ "$NAME" != "${SUPERFAN_PLANT_FROM:-}" ] || started=1
  [ -z "${SUPERFAN_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${SUPERFAN_PLANT_ONLY:-}" ] || case " $SUPERFAN_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
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
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_superfan_fight_diff_test" "$W/build/mystery_superfan_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_superfan_fight_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_superfan_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

FIGHT='/^static void fight(gc_run/,/^}/'
MEMBER='/^static bool make_member/,/^}/'
plant bp-30 's#{30, 50, 70, 100, 120, 140, 160}#{39, 50, 70, 100, 120, 140, 160}#'
plant bp-50 's#{30, 50, 70, 100, 120, 140, 160}#{30, 59, 70, 100, 120, 140, 160}#'
plant bp-70 's#{30, 50, 70, 100, 120, 140, 160}#{30, 50, 79, 100, 120, 140, 160}#'
plant bp-100 's#{30, 50, 70, 100, 120, 140, 160}#{30, 50, 70, 109, 120, 140, 160}#'
plant bp-120 's#{30, 50, 70, 100, 120, 140, 160}#{30, 50, 70, 100, 129, 140, 160}#'
plant bp-140 's#{30, 50, 70, 100, 120, 140, 160}#{30, 50, 70, 100, 120, 149, 160}#'
plant bp-160 's#{30, 50, 70, 100, 120, 140, 160}#{30, 50, 70, 100, 120, 140, 169}#'
plant band1-size 's#^    {3, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}, {M_POOL1, false, false}}},#    {2, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}, {M_POOL1, false, false}}},#'
plant band2-pool 's#{M_POOL1, false, false}, {M_POOL2, false, false}}},#{M_POOL1, false, false}, {M_POOL1, false, false}}},#'
plant band3-pool 's#{M_POOL1, false, false}, {M_POOL2, false, false}, {M_POOL2, false, false}}},#{M_POOL1, false, false}, {M_POOL2, false, false}, {M_POOL1, false, false}}},#'
plant band4-form 's#^    {5, 0, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}}},#    {5, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}}},#'
plant band4-pool3 's#{M_POOL2, false, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}}},#{M_POOL2, false, false}, {M_POOL2, false, false}, {M_POOL3B, false, false}}},#'
plant band5-pool3 's#{M_POOL2, false, false}, {M_POOL3A, false, false}, {M_POOL3B, false, false}}},#{M_POOL2, false, false}, {M_POOL3B, false, false}, {M_POOL3A, false, false}}},#'
plant band5-butterfree 's#^    {5, 0, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}, {M_POOL3B#    {5, 0, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, false, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}, {M_POOL3B#'
plant band6-strong 's#^    {5, 1, {{M_BEEDRILL, true, false}#    {5, 0, {{M_BEEDRILL, true, false}#'
plant band6-pool4 's#{M_POOL3A, false, false}, {M_POOL4, false, false}}},#{M_POOL3A, false, false}, {M_POOL2, false, false}}},#'
plant band7-boss-beedrill 's#{M_BEEDRILL, true, true}#{M_BEEDRILL, true, false}#'
plant band7-boss-butterfree 's#{M_BUTTERFREE, true, true}#{M_BUTTERFREE, true, false}#'
plant band7-strong 's#^    {5, 1, {{M_BEEDRILL, true, true}#    {5, 0, {{M_BEEDRILL, true, true}#'
plant band7-pool3 's#{M_POOL3A, false, false}, {M_POOL3B, false, false}, {M_POOL4, false, false}}},#{M_POOL3B, false, false}, {M_POOL3A, false, false}, {M_POOL4, false, false}}},#'
plant pool1-size "$MEMBER"'s#POOL1\[seed_item(&run->run.rng, 23)\]#POOL1[seed_item(\&run->run.rng, 22)]#'
plant pool2-size "$MEMBER"'s#POOL2\[seed_item(&run->run.rng, 23)\]#POOL2[seed_item(\&run->run.rng, 22)]#'
plant pool4-size "$MEMBER"'s#POOL4\[seed_item(&run->run.rng, 4)\]#POOL4[seed_item(\&run->run.rng, 3)]#'
plant pool1-pool2 "$MEMBER"'s#case M_POOL1: species = POOL1\[#case M_POOL1: species = POOL2[#'
plant pool3-top 's#^    for (int i = 7; i > 0; i--) {#    for (int i = 6; i > 0; i--) {#'
plant pool3-range 's#const int j = pr_rng_integer_in_range(&r, 0, i);#const int j = pr_rng_integer_in_range(\&r, 0, i - 1);#'
plant pool3-first 's#^    out\[0\] = a\[7\];#    out[0] = a[6];#'
plant pool3-second 's#^    out\[1\] = a\[6\];#    out[1] = a[5];#'
plant pool3-stream 's#pr_rng r = pr_run_seed_offset_rng((pr_run \*)&run->run, run->wave, NULL);#pr_rng r = pr_run_seed_offset_rng((pr_run *)\&run->run, run->wave + 1, NULL);#'
plant pool3-form 's#{SPECIES_ID_ORBEETLE, 1},#{SPECIES_ID_ORBEETLE, 0},#'
plant pool3-durant 's#{SPECIES_ID_DURANT, 0}#{SPECIES_ID_DURANT, 1}#'
plant tutor-size "$FIGHT"'s#TUTOR\[k\]\[pr_rng_seed_int(&opt, 5, 0)\]#TUTOR[k][pr_rng_seed_int(\&opt, 4, 0)]#'
plant tutor-stream "$FIGHT"'s#pr_rng opt = gc_me_option_rng(run);#pr_rng opt = run->run.rng;#'
plant tutor-physical 's#MOVE_ID_MEGAHORN, MOVE_ID_ATTACK_ORDER#MOVE_ID_ATTACK_ORDER, MOVE_ID_MEGAHORN#'
plant tutor-special 's#MOVE_ID_SILVER_WIND, MOVE_ID_SIGNAL_BEAM#MOVE_ID_SIGNAL_BEAM, MOVE_ID_SILVER_WIND#'
plant tutor-status 's#MOVE_ID_STRING_SHOT, MOVE_ID_DEFEND_ORDER#MOVE_ID_DEFEND_ORDER, MOVE_ID_STRING_SHOT#'
plant tutor-misc 's#MOVE_ID_QUIVER_DANCE, MOVE_ID_INFESTATION#MOVE_ID_INFESTATION, MOVE_ID_QUIVER_DANCE#'
plant tutor-last 's#MOVE_ID_QUIVER_DANCE, MOVE_ID_INFESTATION}};#MOVE_ID_QUIVER_DANCE, MOVE_ID_QUIVER_DANCE}};#'
plant tutor-order "$FIGHT"'s#for (int k = 0; k < 4; k++) in->superfan.tutor\[k\] = TUTOR\[k\]#for (int k = 3; k >= 0; k--) in->superfan.tutor[k] = TUTOR[k]#'
plant seed-wave "$FIGHT"'s#run->wave + (trainer << 10) + ((e + 1) << 8)#run->wave + (trainer << 10) + (e << 8)#'
plant seed-type "$FIGHT"'s#run->wave + (trainer << 10) + ((e + 1) << 8)#run->wave + (trainer << 9) + ((e + 1) << 8)#'
plant seed-base "$FIGHT"'s#run->wave + (trainer << 10) + ((e + 1) << 8)#(trainer << 10) + ((e + 1) << 8)#'
plant seed-pool3b "$FIGHT"'s#slot->kind == M_POOL3B ? &p3\[1\] : &p3\[0\]#\&p3[0]#'
plant seed-restore "$FIGHT"'s#^    run->run.rng = saved;#    (void)saved;#'
plant member-boss "$MEMBER"'s#if (slot->boss) g.boss_segments = 2;#if (slot->boss) g.boss_segments = 1;#'
plant member-form "$MEMBER"'s#else if (slot->form) form = 1;#else if (slot->form) form = 2;#'
plant member-p3-form "$MEMBER"'s#form = p3->form;#form = 0;#'
plant member-regen "$MEMBER"'s#if (made \&\& gc_member_regen#if (0 \&\& gc_member_regen#'
plant member-ctx-slot "$MEMBER"'s#ec.trainer_slot = TRAINER_SLOT_TRAINER;#ec.trainer_slot = TRAINER_SLOT_NONE;#'
plant member-ec-boss "$MEMBER"'s#ec.boss = false;#ec.boss = true;#'
plant member-moves "$MEMBER"'s#const bool made = gc_generate_moveset(run, &run->run.rng, out);#const bool made = false;#'
plant tpl-strong 's#t->strengths\[i\] = strong ? PARTY_MEMBER_STRENGTH_STRONG : PARTY_MEMBER_STRENGTH_AVERAGE;#t->strengths[i] = PARTY_MEMBER_STRENGTH_AVERAGE;#'
plant tpl-last 's#const bool strong = i >= t->size - BAND\[band\].n_strong;#const bool strong = i > t->size - BAND[band].n_strong;#'
plant fight-trainer "$FIGHT"'s#const int trainer = TRAINER_TYPE_BUG_TYPE_SUPERFAN;#const int trainer = TRAINER_TYPE_BUG_TYPE_SUPERFAN + 1;#'
plant fight-variant "$FIGHT"'s#run->trainer_variant = TRAINER_VARIANT_FEMALE;#run->trainer_variant = TRAINER_VARIANT_DEFAULT;#'
plant fight-double '/^    if (!ok) {/,/^}/s#^    run->double_battle = false;#    run->double_battle = true;#'
plant fight-me-battle "$FIGHT"'s#run->me_battle = true;#run->me_battle = false;#'
plant fight-finish "$FIGHT"'s#gc_enemy_finish_party_me(run);#gc_enemy_finish_party(run);#'
plant fight-reward-fill "$FIGHT"'s#in->me_reward.fill = true;#in->me_reward.fill = false;#'
plant fight-reward-active "$FIGHT"'s#in->me_reward.active = true;#in->me_reward.active = false;#'
plant fight-reward-before "$FIGHT"'s#in->me_reward.before = false;#in->me_reward.before = true;#'
plant fight-reward-func "$FIGHT"'s#in->me_reward.func = -1;#in->me_reward.func = 0;#'
plant fight-catch "$FIGHT"'s#run->me_catch_allowed = false;#run->me_catch_allowed = true;#'
plant fight-nostats "$FIGHT"'s#in->me_no_stats = false;#in->me_no_stats = true;#'
plant fight-tera "$FIGHT"'s#in->me_tera_static = 0;#in->me_tera_static = 1;#'
plant fight-name "$FIGHT"'s#\$t(trainerNames:bugTypeSuperfan)#$t(trainerNames:bugTypeSuperfan2)#'
plant fight-post "$FIGHT"'s#in->me_post.pending = true;#in->me_post.pending = false;#'
plant fight-post-type "$FIGHT"'s#in->me_post.a = GC_ME_BUG_TYPE_SUPERFAN;#in->me_post.a = GC_ME_DARK_DEAL;#'
plant exec-option "s#if (option == 0) { fight(run); return true; }#if (option == 1) { fight(run); return true; }#"
plant pool-trainer-battle 's#ic.trainer_battle = run->battle_type == BATTLE_TYPE_TRAINER \&\& !run->me_battle;#ic.trainer_battle = run->battle_type == BATTLE_TYPE_TRAINER;#' gc_encounter.c
plant pool-thresholds 's#const int pool = run->battle_type == BATTLE_TYPE_TRAINER \&\& !run->me_battle ? 1 : 0;#const int pool = run->battle_type == BATTLE_TYPE_TRAINER ? 1 : 0;#' gc_encounter.c
plant band-export 's#^int gc_superfan_band(int wave) { return band_of(wave); }#int gc_superfan_band(int wave) { return band_of(wave + 1); }#'

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
