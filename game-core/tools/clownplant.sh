#!/bin/sh
# Clowning Around (src/gc_clown.c): show the comparator (and the host test) can fail. Replays one trace (from tools/clowndiff.sh) through copies of game-core, each with one
# line of src/gc_clown.c (or gc_gts.c / gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_clown_diff_test.c exiting 1, or, for what the oracle
# does not play (the heal shop, a save), by tests/mystery_clown_test.c failing.
# Not planted (equivalent, traced): applyModifierTypeToPlayerPokemon's maximum-stack test (every pool item is Ultra or Rogue and so always taken off the holder first, and the pool counts
# already stop each item at its maximum, so the holder never has a stack to top up; the virtual-stack term likewise), generateItemsOfTier's empty-pool test (the draws after it would all find
# the pool empty anyway), the Lucky Egg id test (a tier-less item counts as Ultra, which a Lucky Egg is), the Berry exclusion in the tier loop (a Berry is Common, never taken), the Terastallization
# flag of the types read (nobody is terastallized outside a battle), and the pop's two spellings. The fight's own gates that cannot be reached with the species it has (the stub path
# for a species run-core cannot make; its \`in->n_me_start = 0\`), the config's full-HP reset (the battle's start restores it anyway: traced), the passive flag's reset (a trainer's
# Pokemon is made without one), and the female roll's range (the value is never used; one draw either way) are not planted.
# Usage: tools/clownplant.sh <trace>   (CLOWN_PLANT_FROM=<name> starts at that plant, CLOWN_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-clown-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_clown.c}
  [ "$NAME" != "${CLOWN_PLANT_FROM:-}" ] || started=1
  [ -z "${CLOWN_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${CLOWN_PLANT_ONLY:-}" ] || case " $CLOWN_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_clown_diff_test" "$W/build/mystery_clown_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_clown_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_clown_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

ROLL='/^void gc_clown_roll/,/^}/'
GIVE='/^static bool give/,/^}/'
GEN='/^static int generate_tier/,/^}/'
STACKS='/^static int transferable_stacks/,/^}/'
ITEMS='/^static void shuffle_items/,/^}/'
TYPES='/^static void shuffle_types/,/^}/'
EXEC='/^void gc_clown_execute/,/^}/'
FIGHT='/^static bool build_fight/,/^}/'
START='/^static void fight/,/^}/'
VICT='/^void gc_clown_victory/,/^}/'
POSTS='/^bool gc_clown_post_start/,/^}/'
POSTC='/^bool gc_clown_post_choose/,/^}/'
OUTRO='/^void gc_clown_outro/,/^}/'

plant roll-offset "$ROLL"'s#run->wave, NULL#run->wave + 1, NULL#'
plant roll-ability-count "$ROLL"'s#pr_rng_seed_int(&r, 15, 0)#pr_rng_seed_int(\&r, 14, 0)#'
plant roll-first "$ROLL"'s#const int first = pr_rng_seed_int(&r, 18, 0);#const int first = pr_rng_seed_int(\&r, 17, 0);#'
plant roll-second "$ROLL"'s#int second = pr_rng_seed_int(&r, 17, 0);#int second = pr_rng_seed_int(\&r, 16, 0);#'
plant roll-skip "$ROLL"'s#if (second >= first) second++;#if (second > first) second++;#'
plant ability-first 's#ABILITY_ID_STURDY, ABILITY_ID_PICKUP#ABILITY_ID_GUTS, ABILITY_ID_PICKUP#'
plant ability-mid 's#ABILITY_ID_SAND_STREAM, ABILITY_ID_ELECTRIC_SURGE#ABILITY_ID_DROUGHT, ABILITY_ID_ELECTRIC_SURGE#'
plant ability-last 's#ABILITY_ID_SHEER_FORCE, ABILITY_ID_PRANKSTER#ABILITY_ID_SHEER_FORCE, ABILITY_ID_MAGICIAN#'
plant give-arg "$GIVE"'s#if (!strcmp(it->key, key) \&\& it->arg == arg \&\& it->pokemon_id == to->b.id)#if (!strcmp(it->key, key) \&\& it->pokemon_id == to->b.id)#'
plant gen-index "$GEN"'s#const int idx = pr_rng_seed_int(r, len, 0);#const int idx = len - 1 - pr_rng_seed_int(r, len, 0);#'
plant gen-booster-rng "$GEN"'s#^            \*r = run->run.rng;#            (void)0;#'
plant gen-drop "$GEN"'s#if (--pool\[idx\].left <= 0) {#if (--pool[idx].left < 0) {#'
plant gen-null "$GEN"'s#made = type >= 0;#made = true;#'
plant stacks-berry "$STACKS"'s#strcmp(it->key, "BERRY")) n += it->stack;#1) n += it->stack;#'
plant stacks-count "$STACKS"'s#n += it->stack;#n += 1;#'
plant stacks-transferable "$STACKS"'s#gc_gts_item_transferable(it) \&\& ##'
plant sel-tie "$ITEMS"'s#if (next > count) {#if (next >= count) {#'
plant sel-start "$ITEMS"'s#const gc_mon \*best = \&run->party\[run->party_order\[0\]\];#const gc_mon *best = \&run->party[0];#'
plant sel-order "$ITEMS"'s#const gc_mon \*m = \&run->party\[run->party_order\[k\]\];#const gc_mon *m = \&run->party[k];#'
plant sel-offset "$ITEMS"'s#run->wave \* 1000 + 512#run->wave * 1000 + 513#'
plant berries-count "$ITEMS"'s#n_berries += run->items\[i\].stack; gc_item_remove_at(run, i); }#n_berries += 1; gc_item_remove_at(run, i); }#'
plant berries-keep "$ITEMS"'s#n_berries += run->items\[i\].stack; gc_item_remove_at(run, i); }#n_berries += run->items[i].stack; i++; }#'
plant berries-other "$ITEMS"'s#run->items\[i\].pokemon_id == id \&\& !strcmp(run->items\[i\].key, "BERRY")#!strcmp(run->items[i].key, "BERRY")#'
plant berry-apicot "$ITEMS"'s#{"BERRY", BERRY_TYPE_APICOT, 3}#{"BERRY", BERRY_TYPE_APICOT, 4}#'
plant berry-enigma "$ITEMS"'s#{"BERRY", BERRY_TYPE_ENIGMA, 2}#{"BERRY", BERRY_TYPE_ENIGMA, 3}#'
plant berry-ganlon "$ITEMS"'s#{"BERRY", BERRY_TYPE_GANLON, 3}#{"BERRY", BERRY_TYPE_GANLON, 2}#'
plant berry-lansat "$ITEMS"'s#{"BERRY", BERRY_TYPE_LANSAT, 3}#{"BERRY", BERRY_TYPE_LANSAT, 4}#'
plant berry-leppa "$ITEMS"'s#{"BERRY", BERRY_TYPE_LEPPA, 2}#{"BERRY", BERRY_TYPE_LEPPA, 3}#'
plant berry-liechi "$ITEMS"'s#{"BERRY", BERRY_TYPE_LIECHI, 3}#{"BERRY", BERRY_TYPE_LIECHI, 2}#'
plant berry-lum "$ITEMS"'s#{"BERRY", BERRY_TYPE_LUM, 2}#{"BERRY", BERRY_TYPE_LUM, 3}#'
plant berry-petaya "$ITEMS"'s#{"BERRY", BERRY_TYPE_PETAYA, 3}#{"BERRY", BERRY_TYPE_PETAYA, 2}#'
plant berry-salac "$ITEMS"'s#{"BERRY", BERRY_TYPE_SALAC, 2}#{"BERRY", BERRY_TYPE_SALAC, 3}#'
plant berry-sitrus "$ITEMS"'s#{"BERRY", BERRY_TYPE_SITRUS, 2}#{"BERRY", BERRY_TYPE_SITRUS, 3}#'
plant berry-starf "$ITEMS"'s#{"BERRY", BERRY_TYPE_STARF, 3}#{"BERRY", BERRY_TYPE_STARF, 2}#'
plant berry-order "$ITEMS"'s#{"BERRY", BERRY_TYPE_APICOT, 3}, {"BERRY", BERRY_TYPE_ENIGMA, 2}#{"BERRY", BERRY_TYPE_ENIGMA, 2}, {"BERRY", BERRY_TYPE_APICOT, 3}#'
plant tier-fallback "$ITEMS"'s#tier = TIER_ULTRA;   /\* type.tier#tier = TIER_ROGUE;   /* type.tier#'
plant tier-rogue "$ITEMS"'s#|| tier == TIER_ROGUE) {#|| tier == TIER_ULTRA) {#'
plant tier-ultra "$ITEMS"'s#|| tier == TIER_ULTRA) { n_ultra#|| tier == TIER_ROGUE) { n_ultra#'
plant golden-egg "$ITEMS"'s#if (!strcmp(it->key, "GOLDEN_EGG") || tier == TIER_ROGUE)#if (tier == TIER_ROGUE)#'
plant soothe-bell "$ITEMS"'s#|| !strcmp(it->key, "SOOTHE_BELL") || tier == TIER_ULTRA#|| tier == TIER_ULTRA#'
plant tier-count-rogue "$ITEMS"'s#{ n_rogue += stack; gc_item_remove_at(run, i); }#{ n_rogue += 1; gc_item_remove_at(run, i); }#'
plant tier-count-ultra "$ITEMS"'s#{ n_ultra += stack; gc_item_remove_at(run, i); }#{ n_ultra += 1; gc_item_remove_at(run, i); }#'
plant tier-keep-rogue "$ITEMS"'s#{ n_rogue += stack; gc_item_remove_at(run, i); }#{ n_rogue += stack; i++; }#'
plant tier-keep-ultra "$ITEMS"'s#{ n_ultra += stack; gc_item_remove_at(run, i); }#{ n_ultra += stack; i++; }#'
plant tier-other-holder "$ITEMS"'s#if (it->pokemon_id != id || !gc_gts_item_transferable(it)#if (!gc_gts_item_transferable(it)#'
plant tier-transferable "$ITEMS"'s#|| !gc_gts_item_transferable(it) || !strcmp#|| !strcmp#'
plant ultra-reviver "$ITEMS"'s#{"REVIVER_SEED", -1, 1}#{"REVIVER_SEED", -1, 2}#'
plant ultra-punch "$ITEMS"'s#{"GOLDEN_PUNCH", -1, 5}#{"GOLDEN_PUNCH", -1, 4}#'
plant ultra-booster "$ITEMS"'s#{"ATTACK_TYPE_BOOSTER", -1, 99}#{"ATTACK_TYPE_BOOSTER", -1, 1}#'
plant ultra-claw "$ITEMS"'s#{"QUICK_CLAW", -1, 3}#{"QUICK_CLAW", -1, 4}#'
plant ultra-wide "$ITEMS"'s#{"WIDE_LENS", -1, 3}#{"WIDE_LENS", -1, 2}#'
plant ultra-order "$ITEMS"'s#{"REVIVER_SEED", -1, 1}, {"GOLDEN_PUNCH", -1, 5}#{"GOLDEN_PUNCH", -1, 5}, {"REVIVER_SEED", -1, 1}#'
plant rogue-leftovers "$ITEMS"'s#{"LEFTOVERS", -1, 4}#{"LEFTOVERS", -1, 5}#'
plant rogue-bell "$ITEMS"'s#{"SHELL_BELL", -1, 4}#{"SHELL_BELL", -1, 3}#'
plant rogue-dew "$ITEMS"'s#{"SOUL_DEW", -1, 10}#{"SOUL_DEW", -1, 9}#'
plant rogue-scope "$ITEMS"'s#{"SCOPE_LENS", -1, 1}#{"SCOPE_LENS", -1, 2}#'
plant rogue-baton "$ITEMS"'s#{"BATON", -1, 1}#{"BATON", -1, 2}#'
plant rogue-band "$ITEMS"'s#{"FOCUS_BAND", -1, 5}#{"FOCUS_BAND", -1, 4}#'
plant rogue-rock "$ITEMS"'s#{"KINGS_ROCK", -1, 3}#{"KINGS_ROCK", -1, 4}#'
plant rogue-grip "$ITEMS"'s#{"GRIP_CLAW", -1, 5}#{"GRIP_CLAW", -1, 4}#'
plant rogue-order "$ITEMS"'s#{"LEFTOVERS", -1, 4}, {"SHELL_BELL", -1, 4}#{"SHELL_BELL", -1, 4}, {"LEFTOVERS", -1, 4}#'
plant added-prune "$ITEMS"'s#^    if (added) {#    if (0) {#'
plant added-always "$ITEMS"'s#^    if (added) {#    if (1) {#'
plant text-holder "$ITEMS"'s#pr_arg_pokemon_name(&run->bt, "switchPokemon", &best->b)#pr_arg_pokemon_name(\&run->bt, "switchPokemon", \&run->party[run->party_order[0]].b)#'
plant text-key "$ITEMS"'s#option.2.selected3", 0#option.2.selected2", 0#'
plant types-offset "$TYPES"'s#run->wave \* 1000 + 512#run->wave * 1000 + 513#'
plant types-status "$TYPES"'s#if (pr_moves\[mv\].category == MOVE_CATEGORY_STATUS || has_type#if (has_type#'
plant types-own "$TYPES"'s#|| has_type(original, n_original, t)) continue;#) continue;#'
plant types-dedupe "$TYPES"'s#if (!seen) priority\[n_priority++\] = t;#priority[n_priority++] = t;#'
plant types-sort 's#return strcmp(x, y) < 0;#return a < b;#'
plant types-shuffle "$TYPES"'s#const int j = pr_rng_seed_int(&r, i + 1, 0);#const int j = pr_rng_seed_int(\&r, i, 0);#'
plant types-shuffle-end "$TYPES"'s#for (int i = n_priority - 1; i > 0; i--) {#for (int i = n_priority - 1; i > 1; i--) {#'
plant types-pop "$TYPES"'s#second = priority\[--n_priority\];#second = priority[0];#'
plant types-random "$TYPES"'s#else second = pr_rng_seed_int(&r, 18, 0);#else second = pr_rng_seed_int(\&r, 17, 0);#'
plant types-slot0 "$TYPES"'s#m->b.custom_types\[0\] = 0;#m->b.custom_types[0] = original[0] + 1;#'
plant types-fusion "$TYPES"'s#if (m->b.fusion_species) {#if (0) {#'
plant types-fusion-always "$TYPES"'s#if (m->b.fusion_species) {#if (1) {#'
plant types-fusion-value "$TYPES"'s#m->b.fusion_custom_types\[1\] = (uint8_t)(second + 1);#m->b.fusion_custom_types[1] = (uint8_t)(second + 2);#'
plant types-value "$TYPES"'s#m->b.custom_types\[1\] = (uint8_t)(second + 1);#m->b.custom_types[1] = (uint8_t)(second + 2);#'
plant types-text "$TYPES"'s#option.3.selected2", 1#option.3.selected3", 1#'
plant exec-option "$EXEC"'s#if (option == 1) shuffle_items(run);#if (option == 2) shuffle_items(run);#'
plant exec-leave "$EXEC"'s#^    gc_me_leave_with_heal_shop(run);.*#    (void)0;#'
plant hook-texts-ability 's#^        screen_arg(s, pr_arg_ability("ability", ability));.*#        (void)0;#' gc_mystery.c
plant hook-texts-name 's#^        screen_arg(s, pr_arg_key("blacephalonName", "pokemon:blacephalon"));#        (void)0;#' gc_mystery.c
plant hook-stand-in 's#^        gc_clown_execute(run, option);#        end_mystery_encounter_leave(run);#' gc_mystery.c

# the fight
plant fight-female "$FIGHT"'s#(void)pr_rng_seed_int(&run->run.rng, 2, 0);#(void)0;#'
plant fight-trainer "$FIGHT"'s#const int t = TRAINER_TYPE_HARLEQUIN;#const int t = TRAINER_TYPE_PSYCHIC;#'
plant fight-single "$FIGHT"'s#run->double_battle = true;#run->double_battle = false;#'
plant fight-variant "$FIGHT"'s#run->trainer_variant = TRAINER_VARIANT_DOUBLE;#run->trainer_variant = TRAINER_VARIANT_DEFAULT;#'
plant fight-name "$FIGHT"'s#pr_rng_seed_int(&run->run.rng, info->names, 0) : 0;#pr_rng_seed_int(\&run->run.rng, info->names - 1, 0) : 0;#'
plant fight-name-none "$FIGHT"'s#if (info->names > 0) run->trainer_name = #if (0) run->trainer_name = #'
plant fight-partner "$FIGHT"'s#run->trainer_name = run->trainer_partner_name = -1;#run->trainer_name = -1; run->trainer_partner_name = 0;#'
plant fight-strengths "$FIGHT"'s#strengths\[2\] = {4, 5};#strengths[2] = {4, 4};#'
plant fight-strengths-order "$FIGHT"'s#strengths\[2\] = {4, 5};#strengths[2] = {5, 4};#'
plant fight-size "$FIGHT"'s#run->wave, 2, strengths, 4, true, levels) != 2#run->wave, 1, strengths, 4, true, levels) != 2#'
plant fight-species "$FIGHT"'s#const int species = e ? SPECIES_ID_BLACEPHALON : SPECIES_ID_MR_MIME;#const int species = e ? SPECIES_ID_MR_MIME : SPECIES_ID_BLACEPHALON;#'
plant fight-boss "$FIGHT"'s#levels\[e\], true, true, TRAINER_SLOT_TRAINER#levels[e], false, true, TRAINER_SLOT_TRAINER#'
plant fight-second-iv "$FIGHT"'s#levels\[e\], true, true, TRAINER_SLOT_TRAINER#levels[e], true, false, TRAINER_SLOT_TRAINER#'
plant fight-slot "$FIGHT"'s#levels\[e\], true, true, TRAINER_SLOT_TRAINER#levels[e], true, true, 0#'
plant fight-rng "$FIGHT"'s#^    run->run.rng = r;#    (void)0;#'
plant fight-mime-1 "$FIGHT"'s#{MOVE_ID_TEETER_DANCE, #{MOVE_ID_PSYCHIC, #'
plant fight-mime-2 "$FIGHT"'s#MOVE_ID_ALLY_SWITCH, #MOVE_ID_TEETER_DANCE, #'
plant fight-mime-3 "$FIGHT"'s#MOVE_ID_DAZZLING_GLEAM, #MOVE_ID_ALLY_SWITCH, #'
plant fight-mime-4 "$FIGHT"'s#MOVE_ID_DAZZLING_GLEAM, MOVE_ID_PSYCHIC}#MOVE_ID_DAZZLING_GLEAM, MOVE_ID_ALLY_SWITCH}#'
plant fight-blace-1 "$FIGHT"'s#{MOVE_ID_TRICK, #{MOVE_ID_HYPNOSIS, #'
plant fight-blace-2 "$FIGHT"'s#MOVE_ID_HYPNOSIS, MOVE_ID_SHADOW_BALL#MOVE_ID_TRICK, MOVE_ID_SHADOW_BALL#'
plant fight-blace-3 "$FIGHT"'s#MOVE_ID_SHADOW_BALL, MOVE_ID_MIND_BLOWN#MOVE_ID_PSYCHIC, MOVE_ID_MIND_BLOWN#'
plant fight-blace-4 "$FIGHT"'s#MOVE_ID_SHADOW_BALL, MOVE_ID_MIND_BLOWN}#MOVE_ID_SHADOW_BALL, MOVE_ID_PSYCHIC}#'
plant fight-passive "$FIGHT"'s#m->b.passive_flag = false;#m->b.passive_flag = true;#'
plant fight-status "$FIGHT"'s#m->b.status = STATUS_EFFECT_NONE;#m->b.status = STATUS_EFFECT_BURN;#'
plant fight-custom-who "$FIGHT"'s#^        if (e) {#        if (!e) {#'
plant fight-custom-ability "$FIGHT"'s#m->b.custom_ability = (int16_t)ability;#m->b.custom_ability = (int16_t)ability + 1;#'
plant fight-ability "$FIGHT"'s#m->b.ability = m->b.base_ability = (uint16_t)ability;#m->b.ability = m->b.base_ability = (uint16_t)ability + 1;#'
plant fight-type-first "$FIGHT"'s#custom_types\[0\] = (uint8_t)(first + 1);#custom_types[0] = (uint8_t)first;#'
plant fight-type-second "$FIGHT"'s#custom_types\[1\] = (uint8_t)(second + 1);#custom_types[1] = (uint8_t)second;#'
plant fight-moves "$FIGHT"'s#set_moves(m, e ? blacephalon : mime);#set_moves(m, e ? mime : blacephalon);#'
plant fight-count "$FIGHT"'s#run->n_enemy = 2;#run->n_enemy = 1;#'
plant fight-order "$FIGHT"'s#run->enemy\[0\] = made\[0\];#run->enemy[0] = made[1];#'
plant fight-order-2 "$FIGHT"'s#run->enemy\[1\] = made\[1\];#run->enemy[1] = made[0];#'
plant start-post "$START"'s#in->me_post.pending = true;#in->me_post.pending = false;#'
plant start-post-type "$START"'s#in->me_post.a = GC_ME_CLOWNING_AROUND;#in->me_post.a = GC_ME_A_TRAINERS_TEST;#'
plant start-post-option "$START"'s#in->me_post.b = 0;#in->me_post.b = 1;#'
plant start-post-ability "$START"'s#in->me_post.c = ability;#in->me_post.c = ability + 1;#'
plant start-effect-1 "$START"'s#gc_me_add_start_effect(run, 2, 3, MOVE_ID_ROLE_PLAY);#gc_me_add_start_effect(run, 2, 3, MOVE_ID_TAUNT);#'
plant start-effect-1-source "$START"'s#gc_me_add_start_effect(run, 2, 3, MOVE_ID_ROLE_PLAY);#gc_me_add_start_effect(run, 3, 3, MOVE_ID_ROLE_PLAY);#'
plant start-effect-1-target "$START"'s#gc_me_add_start_effect(run, 2, 3, MOVE_ID_ROLE_PLAY);#gc_me_add_start_effect(run, 2, 2, MOVE_ID_ROLE_PLAY);#'
plant start-effect-2 "$START"'s#gc_me_add_start_effect(run, 3, 0, MOVE_ID_TAUNT);#gc_me_add_start_effect(run, 3, 1, MOVE_ID_TAUNT);#'
plant start-effect-3 "$START"'s#gc_me_add_start_effect(run, 3, 1, MOVE_ID_TAUNT);#gc_me_add_start_effect(run, 3, 1, MOVE_ID_ROLE_PLAY);#'
plant start-effect-3-source "$START"'s#gc_me_add_start_effect(run, 3, 1, MOVE_ID_TAUNT);#gc_me_add_start_effect(run, 2, 1, MOVE_ID_TAUNT);#'
plant start-battle "$START"'s#^    run->me_battle = true;#    run->me_battle = false;#'
plant start-items "$START"'s#^    gc_enemy_finish_party_me(run);#    (void)0;#'
plant start-catch "$START"'s#^    run->me_catch_allowed = false;#    run->me_catch_allowed = true;#'
plant start-reward "$START"'s#^    in->me_reward.active = false;#    in->me_reward.active = true;#'
plant start-stats "$START"'s#^    in->me_no_stats = false;#    in->me_no_stats = true;#'
plant start-type "$START"'s#^    run->mystery_type = run->mystery_tier = -1;#    (void)0;#'
plant start-in-battle "$START"'s#^    run->in_battle = true;#    run->in_battle = false;#'
plant start-flag "$START"'s#^    in->clown.active = true;#    (void)0;#'
plant start-say "$START"'s#^    gc_me_say_trainer_fight(run, in);#    (void)0;#'
plant victory-flag "$VICT"'s#in->clown.active = false;#(void)0;#'
plant victory-end "$VICT"'s#gc_rp(RP_BATTLE_END, 1, 0, 0)#gc_rp(RP_BATTLE_END, 0, 0, 0)#'
plant victory-end-gone "$VICT"'s#^    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));#    (void)0;#'
plant victory-trainer "$VICT"'s#^    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));#    (void)0;#'
plant victory-fill "$VICT"'s#c->fill_remaining = true;#c->fill_remaining = false;#'
plant victory-reroll "$VICT"'s#c->reroll_multiplier = NAN;#c->reroll_multiplier = -1;#'
plant victory-luck "$VICT"'s#c->allow_luck_upgrades = true;#c->allow_luck_upgrades = false;#'
plant victory-shop "$VICT"'s#^    gc_me_end_with_shop(run);#    (void)0;#'
plant exec-fight "$EXEC"'s#if (option == 0) { fight(run); return; }#if (option == 0) { shuffle_types(run); }#'
plant exec-post "$EXEC"'s#in->me_post.pending = true;#in->me_post.pending = false;#'
plant exec-post-type "$EXEC"'s#in->me_post.a = GC_ME_CLOWNING_AROUND;#in->me_post.a = GC_ME_A_TRAINERS_TEST;#'
plant exec-post-option "$EXEC"'s#in->me_post.b = option;#in->me_post.b = 0;#'
plant post-fight "$POSTS"'s#if (p->b != 0) {#if (p->b != 1) {#'
plant post-dialogue "$POSTS"'s#option.1.applyAbilityDialogue", 0#option.1.applyAbilityMessage", 0#'
plant post-message "$POSTS"'s#option.1.applyAbilityMessage", 1, pr_arg_ability("ability", p->c)#option.1.applyAbilityMessage", 1, pr_arg_ability("ability", p->c + 1)#'
plant post-prompt "$POSTS"'s#option.1.abilityPrompt", 1, pr_arg_ability("ability", p->c)#option.1.abilityPrompt", 1, pr_arg_ability("ability", p->c + 1)#'
plant post-stage "$POSTS"'s#s->me_stage = GC_ME_STAGE_ABILITY;#s->me_stage = GC_ME_STAGE_TUTOR;#'
plant post-options "$POSTS"'s#s->n_options = 2;#s->n_options = 3;#'
plant post-ok "$POSTS"'s#s->option_ok\[0\] = s->option_ok\[1\] = true;#s->option_ok[0] = true;#'
plant post-pick "$POSTS"'s#s->option_pick\[0\] = GC_PICK_MEMBER;#s->option_pick[0] = GC_PICK_NONE;#'
plant post-type "$POSTS"'s#s->mystery_type = GC_ME_CLOWNING_AROUND;#s->mystery_type = GC_ME_A_TRAINERS_TEST;#'
plant post-arg-ability "$POSTS"'s#s->me_args\[s->n_me_args++\] = pr_arg_ability("ability", p->c);#s->n_me_args++;#'
plant post-arg-name "$POSTS"'s#s->me_args\[s->n_me_args++\] = pr_arg_key("blacephalonName", "pokemon:blacephalon");#s->n_me_args++;#'
plant post-wait "$POSTS"'s#^    return false;#    return true;#'
plant choose-kind "$POSTC"'s#if (c->kind != GC_CH_MYSTERY) {#if (0) {#'
plant choose-a "$POSTC"'s#if (c->a != 0 \&\& c->a != 1) {#if (0) {#'
plant choose-member "$POSTC"'s#c->b < 0 || c->b >= run->n_party#c->b >= run->n_party#'
plant choose-member-high "$POSTC"'s#c->b < 0 || c->b >= run->n_party#c->b < 0 || c->b > run->n_party#'
plant choose-screen "$POSTC"'s#memset(&run->screen, 0, sizeof run->screen);#(void)0;#'
plant choose-yes "$POSTC"'s#if (c->a == 0) {$#if (c->a == 1) {#'
plant choose-fusion "$POSTC"'s#if (m->b.fusion_species > 0) m->b.fusion_custom_ability#if (0) m->b.fusion_custom_ability#'
plant choose-fusion-always "$POSTC"'s#if (m->b.fusion_species > 0) m->b.fusion_custom_ability#if (1) m->b.fusion_custom_ability#'
plant choose-value "$POSTC"'s#m->b.custom_ability = (int16_t)p->c;#m->b.custom_ability = (int16_t)p->c + 1;#'
plant choose-refresh "$POSTC"'s#pr_refresh_ability(&m->b);#(void)0;#'
plant choose-gained "$POSTC"'s#option.1.abilityGained", 2, pr_arg_pokemon_name(&run->bt, "chosenPokemon", &m->b)#option.1.abilityGained", 2, pr_arg_pokemon_name(\&run->bt, "chosenPokemon", \&run->party[0].b)#'
plant choose-gained-ability "$POSTC"'s#pr_arg_ability("ability", p->c));$#pr_arg_ability("ability", p->c + 1));#'
plant choose-outro "$POSTC"'s#^    gc_clown_outro(run);#    (void)0;#'
plant outro-key "$OUTRO"'s#clowningAround:outro", 0#clowningAround:option.1.abilityGained", 0#'
plant hook-victory 's#^        gc_clown_victory(run);#        (void)0;#' gc_phases.c
plant hook-post-start 's#^        if (p->a == GC_ME_CLOWNING_AROUND) return gc_clown_post_start(run, p);.*#        (void)0;#' gc_phases.c
plant hook-post-choose 's#^        return gc_clown_post_choose(run, p, c, why, ok);#        return false;#' gc_phases.c
plant hook-pick 's#^    if (run->screen.me_stage == GC_ME_STAGE_ABILITY \&\& type == GC_ME_CLOWNING_AROUND) return option == 0 \&\& idx >= 0 \&\& idx < run->n_party;.*#    (void)0;#' gc_mystery.c
plant hook-double-appeared 's#run->double_battle ? "battle:trainerAppearedDouble" : "battle:trainerAppeared"#"battle:trainerAppeared"#' gc_winstrate.c
plant new-enemy-slot 's#^    gc_mon_new_enemy(run, out, species, level, \&g, trainer_slot);#    gc_mon_new_enemy(run, out, species, level, \&g, 0);#' gc_darkdeal.c
plant new-enemy-ctx 's#^    ec.trainer_slot = trainer_slot;#    (void)0;#' gc_darkdeal.c
plant new-enemy-refresh 's#^    gc_refresh_enemy(run, out, species, level, \&g, trainer_slot);#    gc_refresh_enemy(run, out, species, level, \&g, 0);#' gc_darkdeal.c
plant movegen-boss 's#|| m->b.fusion_species || m->b.passive_flag#|| m->b.fusion_species || m->b.is_boss || m->b.passive_flag#' gc_movegen.c

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
