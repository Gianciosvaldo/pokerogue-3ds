#!/bin/sh
# Dancing Lessons: show the comparator (and the host test) can fail. Replays one trace (from tools/dancediff.sh) through copies of game-core, each with one line of src/gc_dance.c (or gc_mystery.c, gc_fof.c, battle-core/src/switching.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_dance_diff_test.c exiting 1, or, for what the oracle does not play (the battle's end, the lesson's order before the shop, the party-full prompt, the refused picks), by tests/mystery_dance_test.c failing.
# Not planted (equivalent mutants): the lesson queued last instead of first (the queue is empty at that point, so both orders give the same queue), the move list's slot bound (a slot past the moveset holds 0 and the dance check refuses it anyway), the moveset pushes for an Oricorio that knows fewer than four moves (every generated wild moveset has four), the sprite and the dance animation, and the equivalent mutants.
# Usage: tools/danceplant.sh <trace>   (DANCE_PLANT_FROM=<name> starts at that plant, DANCE_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-dance-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_dance.c}
  [ "$NAME" != "${DANCE_PLANT_FROM:-}" ] || started=1
  [ -z "${DANCE_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${DANCE_PLANT_ONLY:-}" ] || case " $DANCE_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
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
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_dance_diff_test" "$W/build/mystery_dance_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_dance_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_dance_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

FORM='/^static int form_of_biome/,/^}/'
plant form-temple-baile "$FORM"'s|biome == BIOME_ID_TEMPLE \|\| biome == BIOME_ID_TALL_GRASS) return 0;|biome == BIOME_ID_TEMPLE \|\| biome == BIOME_ID_TALL_GRASS) return 1;|'
plant form-tall-grass "$FORM"'s|biome == BIOME_ID_TEMPLE \|\| biome == BIOME_ID_TALL_GRASS|biome == BIOME_ID_TEMPLE \|\| biome == BIOME_ID_FOREST|'
plant form-beach "$FORM"'s|biome == BIOME_ID_BEACH \|\| biome == BIOME_ID_GRASS) return 1;|biome == BIOME_ID_BEACH \|\| biome == BIOME_ID_GRASS) return 0;|'
plant form-grass "$FORM"'s|biome == BIOME_ID_BEACH \|\| biome == BIOME_ID_GRASS|biome == BIOME_ID_BEACH \|\| biome == BIOME_ID_FOREST|'
plant form-island "$FORM"'s|biome == BIOME_ID_ISLAND \|\| biome == BIOME_ID_RUINS) return 2;|biome == BIOME_ID_ISLAND \|\| biome == BIOME_ID_RUINS) return 3;|'
plant form-ruins "$FORM"'s|biome == BIOME_ID_ISLAND \|\| biome == BIOME_ID_RUINS|biome == BIOME_ID_ISLAND \|\| biome == BIOME_ID_FOREST|'
plant form-graveyard "$FORM"'s|biome == BIOME_ID_GRAVEYARD \|\| biome == BIOME_ID_BADLANDS) return 3;|biome == BIOME_ID_GRAVEYARD \|\| biome == BIOME_ID_BADLANDS) return 2;|'
plant form-badlands "$FORM"'s|biome == BIOME_ID_GRAVEYARD \|\| biome == BIOME_ID_BADLANDS|biome == BIOME_ID_GRAVEYARD \|\| biome == BIOME_ID_FOREST|'
plant form-random-range 's|form = pr_rng_seed_int(\&r, 4, 0);|form = pr_rng_seed_int(\&r, 3, 0);|'
plant form-random-lost 's|form = pr_rng_seed_int(\&r, 4, 0);|form = 0;|'
plant form-assigned 's|    o->b.form = (uint8_t)form;|    o->b.form = (uint8_t)(form ^ 1);|'
plant init-stream 's|pr_run_seed_offset_rng(\&run->run, run->wave, NULL)|pr_run_seed_offset_rng(\&run->run, run->wave + 1, NULL)|'
plant init-level 's|    d->level = gc_me_boosted_level(run, \&r);|    d->level = gc_me_boosted_level(run, \&r) + 1;|'
plant init-boss 's|d->level, false, false, \&d->mon)|d->level, true, false, \&d->mon)|'
plant init-second-iv 's|d->level, false, false, \&d->mon)|d->level, false, true, \&d->mon)|'
plant init-species 's|SPECIES_ID_ORICORIO, d->level|SPECIES_ID_PIKACHU, d->level|'
plant init-known-check 's|knows = knows \|\| o->b.moves\[k\] == MOVE_ID_REVELATION_DANCE;|knows = false;|'
plant init-replaced-slot 's|        else gc_mon_set_move(o, 0, MOVE_ID_REVELATION_DANCE);|        else gc_mon_set_move(o, 1, MOVE_ID_REVELATION_DANCE);|'
plant init-stats-lost 's|    pr_calculate_stats(\&run->bt, \&o->b);|    (void)run;|'
plant init-hp-not-full 's|    o->b.hp = o->b.stats\[STAT_HP\];|    o->b.hp = o->b.stats[STAT_HP] - 1;|'
plant init-no-party-guard 's|    if (run->n_party < 1) return false;|    if (run->n_party < 0) return false;|'
plant fight-baton 's|gc_reward_item_index("BATON")|gc_reward_item_index("LEFTOVERS")|'
plant fight-reward-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant fight-reward-active 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant fight-reward-before 's|    in->me_reward.before = false;|    in->me_reward.before = true;|'
plant fight-reward-item 's|    in->me_reward.has_item = false;|    in->me_reward.has_item = true;|'
plant fight-effect-source 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_REVELATION_DANCE);|gc_me_add_start_effect(run, 1, 0, MOVE_ID_REVELATION_DANCE);|'
plant fight-effect-target 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_REVELATION_DANCE);|gc_me_add_start_effect(run, 2, 1, MOVE_ID_REVELATION_DANCE);|'
plant fight-effect-move 's|gc_me_add_start_effect(run, 2, 0, MOVE_ID_REVELATION_DANCE);|gc_me_add_start_effect(run, 2, 0, MOVE_ID_DRAGON_DANCE);|'
plant fight-post-summon 's|\&d->mon, PR_ME_POST_DANCE, \&run->run.rng|\&d->mon, PR_ME_POST_NONE, \&run->run.rng|'
plant fight-own-stream 's|PR_ME_POST_DANCE, \&run->run.rng, GC_ME_DANCING_LESSONS|PR_ME_POST_DANCE, NULL, GC_ME_DANCING_LESSONS|'
plant battle-data-level 's|    if (wave_level) {|    if (0) {|' gc_fof.c
plant battle-level-off 's|        level = battle_level;|        level = battle_level + 1;|' gc_fof.c
plant catch-lost 's|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));|    (void)run;|'
plant catch-obtained-text 's|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 1));|'
plant catch-ball 's|    gc_int(run)->thrown_ball = POKEBALL_TYPE_POKEBALL;|    gc_int(run)->thrown_ball = POKEBALL_TYPE_GREAT_BALL;|'
plant catch-enemy-items-kept 's|    run->n_enemy_items = 0;|    (void)run;|'
plant lesson-member 's|RP_LEARN_MOVE, b, MOVE_ID_REVELATION_DANCE, 0|RP_LEARN_MOVE, 0, MOVE_ID_REVELATION_DANCE, 0|'
plant lesson-move 's|RP_LEARN_MOVE, b, MOVE_ID_REVELATION_DANCE, 0|RP_LEARN_MOVE, b, MOVE_ID_SWORDS_DANCE, 0|'
plant lesson-lost 's|        gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, b, MOVE_ID_REVELATION_DANCE, 0));|        (void)b;|'
plant lesson-no-shop '/RP_LEARN_MOVE/{n;s|gc_me_leave_with_heal_shop(run);|(void)run;|}'
plant show-slot-replaced 's|        else gc_mon_set_move(o, 3, move);|        else gc_mon_set_move(o, 2, move);|'
plant show-known-check 's|knows = knows \|\| o->b.moves\[k\] == move;|knows = false;|'
plant show-no-shop '/catch_it(run, o);/{n;s|gc_me_leave_with_heal_shop(run);|(void)run;|}'
plant show-pick-unfiltered 's|    if (option != 0 \&\& !gc_dance_pick_ok(run, option, b)) return false;|    if (option == 1 \&\& !gc_dance_pick_ok(run, option, b)) return false;|'
plant show-move-unchecked 's|    if (option == 2 \&\& !gc_dance_move_ok(run, b, c)) return false;|    if (0 \&\& !gc_dance_move_ok(run, b, c)) return false;|'
plant stub-silent 's|        gc_stub(run, GC_STUB_UNPORTED_ENEMY);|        (void)run;|'
plant pick-lesson-fainted 's|    if (option == 1) return true;   /\* selectPokemonForOption|    if (option == 1) return run->party[idx].b.hp > 0; /* selectPokemonForOption|'
plant pick-show-option 's|    return option == 2 \&\& gc_dance_has_move(\&run->party\[idx\]);|    return option == 1 \&\& gc_dance_has_move(\&run->party[idx]);|'
plant pick-range 's|    if (idx < 0 \|\| idx >= run->n_party) return false;|    if (idx < 0) return false;|'
plant move-not-a-dance 's|    return run->party\[member\].b.moves\[slot\] > 0 \&\& gc_is_dance_move(run->party\[member\].b.moves\[slot\]);|    return run->party[member].b.moves[slot] > 0;|'
plant dance-aqua-step 's|    return move == MOVE_ID_AQUA_STEP|    return move == MOVE_ID_TACKLE|'
plant dance-fiery-list 's|move == MOVE_ID_FIERY_DANCE \|\| move == MOVE_ID_LUNAR_DANCE|move == MOVE_ID_TACKLE \|\| move == MOVE_ID_LUNAR_DANCE|'
plant dance-quiver 's|move == MOVE_ID_QUIVER_DANCE|move == MOVE_ID_TACKLE|'
plant has-move-allowed 's|    if (!gc_mon_allowed(m)) return false;|    (void)m;|'
plant first-move-token 's|gc_is_dance_move(m->b.moves\[k\])) return m->b.moves\[k\];|gc_is_dance_move(m->b.moves[k])) return MOVE_ID_REVELATION_DANCE;|'
POST='/p->me_post_summon == PR_ME_POST_DANCE/,/pr_unshift_stat_stage_change/'
plant post-count "$POST"'s|stats, 4, 1, p|stats, 3, 1, p|' battle-core/src/switching.c
plant post-amount "$POST"'s|stats, 4, 1, p|stats, 4, 2, p|' battle-core/src/switching.c
plant post-stat "$POST"'s|{STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF}|{STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPD}|' battle-core/src/switching.c
plant post-text "$POST"'s|dancingLessons:option.1.bossEnraged|dancingLessons:option.1.statBoost|' battle-core/src/switching.c
plant post-token "$POST"'s|pr_arg_key("oricorioName"|pr_arg_key("enemyPokemon"|' battle-core/src/switching.c
plant post-not-spent "$POST"'s|        p->me_post_summon = PR_ME_POST_NONE;|        (void)p;|' battle-core/src/switching.c
plant post-silent "$POST"'s|        pr_queue_msg(bt, e, pr_msg1(|        (void)(pr_msg1(|' battle-core/src/switching.c
SCREEN='/case GC_ME_DANCING_LESSONS: {/,/break;/'
plant ui-pick-second "$SCREEN"'s|s->option_pick\[1\] = GC_PICK_MEMBER;|s->option_pick[1] = 0;|' gc_mystery.c
plant ui-pick-third "$SCREEN"'s|s->option_pick\[2\] = GC_PICK_MEMBER_MOVE;|s->option_pick[2] = GC_PICK_MEMBER;|' gc_mystery.c
plant ui-token-move "$SCREEN"'s|gc_dance_first_move(\&run->party\[p\])|MOVE_ID_REVELATION_DANCE|' gc_mystery.c
plant ui-token-name "$SCREEN"'s|"option3PrimaryName"|"option2PrimaryName"|' gc_mystery.c
plant ui-option-ok '/else if (type == GC_ME_DANCING_LESSONS) {/{n;n;s|>= 0;|>= 1;|}' gc_mystery.c
plant primary-option '/if (type == GC_ME_DANCING_LESSONS) {/{n;n;s|option != 2|option != 1|}' gc_mystery.c
plant primary-order 's|if (gc_dance_has_move(\&run->party\[run->party_order\[k\]\])) return run->party_order\[k\];|if (gc_dance_has_move(\&run->party[k])) return k;|' gc_mystery.c
plant ui-move-list 's|return option == 2 \&\& gc_dance_move_ok(run, member, slot);|return option == 1 \&\& gc_dance_move_ok(run, member, slot);|' gc_mystery.c
plant ui-pick-ok 's|    if (type == GC_ME_DANCING_LESSONS) return gc_dance_pick_ok(run, option, idx);|    if (type == GC_ME_DANCING_LESSONS) return true;|' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
