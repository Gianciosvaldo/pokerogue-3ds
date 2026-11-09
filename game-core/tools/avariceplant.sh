#!/bin/sh
# Absolute Avarice: show the comparator (and the host test) can fail. Replays one trace (from tools/avaricediff.sh) through copies of
# game-core, each with one line of src/gc_avarice.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by
# tests/mystery_avarice_diff_test.c exiting 1, or, for what the oracle does not play (the rewards phase reaching the callback, and the catch of a full party), by tests/mystery_avarice_test.c failing.
# Not planted: the choice of the first or the last of equal highest levels in the feast (only the level is used, so it cannot show), battle-core's post-summon handler itself (its stage changes are compared through the Sp. Def and Speed stages of the differential), the cap on a Berry the plea would hand back (a Pokemon holds none when it gets them, so the check can never refuse: an equivalent change), the Greedent's sprites and the sounds (the app's).
# Usage: tools/avariceplant.sh <trace>   (AVARICE_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-avarice-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_avarice.c}
  [ "$NAME" != "${AVARICE_PLANT_FROM:-}" ] || started=1
  [ -z "${AVARICE_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_avarice_diff_test" "$W/build/mystery_avarice_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_avarice_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_avarice_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
plant berry-cap-leppa 's/type == BERRY_TYPE_LUM || type == BERRY_TYPE_LEPPA || type == BERRY_TYPE_ENIGMA ? 2 : 3;/type == BERRY_TYPE_LUM || type == BERRY_TYPE_ENIGMA ? 2 : 3;/'
plant berry-cap-three 's/type == BERRY_TYPE_ENIGMA ? 2 : 3;/type == BERRY_TYPE_ENIGMA ? 3 : 3;/'
plant boss-merge-nocap 's/} else if (out\[k\].stack < berry_cap(it->arg)) {/} else if (1) {/'
plant boss-merge-none 's/while (k < n \&\& out\[k\].arg != it->arg) k++;/while (k < n \&\& 0) k++;/'
plant boss-items-after-steal 's/    const int n_items = boss_items(run, items);/    steal_berries(run); const int n_items = boss_items(run, items);/'
plant steal-leppa-kept 's/        if (is_berry(\&run->items\[i\])) gc_item_remove_at(run, i);/        if (is_berry(\&run->items[i]) \&\& run->items[i].arg != BERRY_TYPE_LEPPA) gc_item_remove_at(run, i);/'
plant steal-forgotten 's/^    steal_berries(run);/    (void)0;/'
plant level-boost 's|floor((double)run->wave / 10 \* 1.0 + 0.5), 0);|floor((double)run->wave / 10 * 0.5 + 0.5), 0);|'
plant level-rounding 's|floor((double)run->wave / 10 \* 1.0 + 0.5), 0);|floor((double)run->wave / 10 * 1.0), 0);|'
plant level-stream 's|    pr_rng r = run->run.rng;|    pr_rng r = gc_me_option_rng(run);|' 
plant second-iv-roll 's|SPECIES_ID_GREEDENT, level, true, boss)|SPECIES_ID_GREEDENT, level, false, boss)|'
plant rng-not-kept 's|    run->run.rng = r;|    (void)r;|'
plant bars-four 's|    boss->b.boss_segments = 3;|    boss->b.boss_segments = 4;|'
plant bar-index 's|    boss->b.boss_segment_index = 2;|    boss->b.boss_segment_index = 1;|'
plant not-a-boss 's|    boss->b.is_boss = true;|    boss->b.is_boss = false;|'
plant boss-passive 's|    boss->b.passive_flag = false;|    boss->b.passive_flag = true;|'
plant shiny-kept 's|    boss->b.shiny = false;|    boss->b.shiny = true;|'
plant move-thrash 's|gc_mon_set_move(boss, 0, MOVE_ID_THRASH);|gc_mon_set_move(boss, 0, MOVE_ID_OUTRAGE);|'
plant move-crunch 's|gc_mon_set_move(boss, 1, MOVE_ID_CRUNCH);|gc_mon_set_move(boss, 1, MOVE_ID_BITE);|'
plant move-body-press 's|gc_mon_set_move(boss, 2, MOVE_ID_BODY_PRESS);|gc_mon_set_move(boss, 2, MOVE_ID_BODY_SLAM);|'
plant move-slack-off 's|gc_mon_set_move(boss, 3, MOVE_ID_SLACK_OFF);|gc_mon_set_move(boss, 3, MOVE_ID_REST);|'
plant move-order 's|gc_mon_set_move(boss, 0, MOVE_ID_THRASH);|gc_mon_set_move(boss, 0, MOVE_ID_CRUNCH);|'
plant post-summon-lost 's|    boss->b.me_post_summon = run->wave < 50 ? PR_ME_POST_AVARICE_LOW : PR_ME_POST_AVARICE_HIGH;|    boss->b.me_post_summon = PR_ME_POST_NONE;|'
plant post-summon-threshold 's|run->wave < 50 ? PR_ME_POST_AVARICE_LOW|run->wave < 60 ? PR_ME_POST_AVARICE_LOW|'
plant post-summon-wrong 's|PR_ME_POST_AVARICE_HIGH;|PR_ME_POST_BREED_HIGH;|'
plant items-count 's|    gc_enemy_finish_party_items(run, items, n_items);|    gc_enemy_finish_party_items(run, items, n_items > 0 ? n_items - 1 : 0);|'
plant reward-forgotten 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant reward-no-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant reward-callback-after 's|    in->me_reward.before = true;   /\* the callback setEncounterRewards was given \*/|    in->me_reward.before = false;|'
plant reward-pending-lost 's|    in->avarice.pending = true;|    in->avarice.pending = false;|'
plant reward-guard-lost 's|    if (!in->avarice.pending) return;|    (void)in;|'
plant flee-allowed 's|    in->me_no_flee = true;|    in->me_no_flee = false;|'
plant switch-blocked 's|    in->me_no_flee = true;|    in->me_no_flee = true; in->me_no_switch = true;|'
plant start-stuff-cheeks-missing 's|    gc_me_add_start_effect(run, 2, 2, MOVE_ID_STUFF_CHEEKS);|    (void)0;|'
plant start-target 's|    gc_me_add_start_effect(run, 2, 2, MOVE_ID_STUFF_CHEEKS);|    gc_me_add_start_effect(run, 2, 0, MOVE_ID_STUFF_CHEEKS);|'
plant start-move 's|    gc_me_add_start_effect(run, 2, 2, MOVE_ID_STUFF_CHEEKS);|    gc_me_add_start_effect(run, 2, 2, MOVE_ID_STOCKPILE);|'
plant seed-holder-ignored 's/revives |= run->items\[i\].pokemon_id == m->b.id \&\& !strcmp(/revives |= !strcmp(/'
plant seed-always 's|        if (!revives) gc_item_add(run, "REVIVER_SEED", -1, m->b.id, 1);|        gc_item_add(run, "REVIVER_SEED", -1, m->b.id, 1);|'
plant stash-text 's|":option.1.foodStash"|":option.1.foodstash"|' 
plant stash-token 's|pr_arg_key("foodReward", "modifierType:ModifierType.REVIVER_SEED.name")|pr_arg_key("foodReward", "modifierType:ModifierType.LEFTOVERS.name")|'
plant plea-fraction 's|floor((double)n \* 2 / 5)|floor((double)n * 3 / 5)|'
plant plea-rounding 's|floor((double)n \* 2 / 5)|ceil((double)n * 2 / 5)|'
plant plea-no-shuffle 's|const int t = (int)floor(pr_rng_frac(\&r) \* (j + 1));|const int t = j;|'
plant plea-shuffle-bound 's|const int t = (int)floor(pr_rng_frac(\&r) \* (j + 1));|const int t = (int)floor(pr_rng_frac(\&r) * j);|'
plant plea-shuffle-direction 's|for (int j = n - 1; j > 0; j--) {|for (int j = 1; j < n; j++) {|'
plant plea-pop-first 's|const int type = list\[--n\];|const int type = list[0]; for (int q = 0; q + 1 < n; q++) list[q] = list[q + 1]; n--;|'
plant plea-stream '/^static void reason/,/^}/ s|    pr_rng r = gc_me_option_rng(run);|    pr_rng r = run->run.rng;|'
plant plea-no-heal-shop '/^static void reason/,/^}/ s|    gc_me_leave_with_heal_shop(run);|    gc_me_leave(run);|'
plant plea-party-storage-order '/^static void reason/,/^}/ s|const gc_mon \*m = \&run->party\[run->party_order\[k\]\];|const gc_mon *m = \&run->party[k];|'
plant feast-level 's|run->party\[high\].b.level - 2 : 1;|run->party[high].b.level - 1 : 1;|'
plant feast-skip-fainted 's|        const int idx = run->party_order\[k\];|        const int idx = run->party_order[k]; if (!gc_mon_allowed(\&run->party[idx])) continue;|'
plant feast-shiny-lock 's|    ec.shiny_lock = true;|    ec.shiny_lock = false;|'
plant feast-passive 's|    greedent.b.passive_flag = true;|    greedent.b.passive_flag = false;|'
plant feast-move-thrash 's|gc_mon_set_move(\&greedent, 0, MOVE_ID_THRASH);|gc_mon_set_move(\&greedent, 0, MOVE_ID_OUTRAGE);|'
plant feast-move-body-press 's|gc_mon_set_move(\&greedent, 1, MOVE_ID_BODY_PRESS);|gc_mon_set_move(\&greedent, 1, MOVE_ID_BODY_SLAM);|'
plant feast-move-stuff-cheeks 's|gc_mon_set_move(\&greedent, 2, MOVE_ID_STUFF_CHEEKS);|gc_mon_set_move(\&greedent, 2, MOVE_ID_STOCKPILE);|'
plant feast-move-slack-off 's|gc_mon_set_move(\&greedent, 3, MOVE_ID_SLACK_OFF);|gc_mon_set_move(\&greedent, 3, MOVE_ID_REST);|'
plant feast-ball 's|POKEBALL_TYPE_POKEBALL;|POKEBALL_TYPE_GREAT_BALL;|'
plant feast-catch-text 's|gc_rp(RP_CATCH, 0, 0, 2)|gc_rp(RP_CATCH, 0, 0, 1)|'
plant feast-no-catch 's|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));|    (void)0;|'
plant feast-no-heal-shop '/gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));/{n;s|gc_me_leave_with_heal_shop(run);|gc_me_leave(run);|}'
plant feast-stats-not-made 's|    pr_calculate_stats(\&run->bt, \&out->b);|    (void)0;|'
plant feast-stream '/^static void let_it_have_it/,/^}/ s|    pr_rng r = gc_me_option_rng(run);|    pr_rng r = run->run.rng;|'
plant option-swapped 's|    if (option == 0) fight(run, items, n_items);|    if (option == 1) fight(run, items, n_items);|'
plant token-name 's|pr_arg_key("greedentName", "pokemon:greedent");|pr_arg_key("greedentNam", "pokemon:greedent");|'
plant plea-gated 's|        gc_avarice_tokens(run, \&name);|        gc_avarice_tokens(run, \&name); s->option_ok[1] = false;|' gc_mystery.c
plant callback-not-run 's|gc_berries_reward(run); gc_avarice_reward(run); gc_fiery_reward(run);|gc_berries_reward(run); gc_fiery_reward(run);|' gc_phases.c
plant spawn-pouch-counts '/^bool gc_avarice_spawnable/,/^}/ s|if (is_berry(\&run->items\[i\])) n +=|if (strstr(run->items[i].key, "BERRY")) n +=|'
plant spawn-threshold 's|    return n >= 6;|    return n >= 5;|'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
