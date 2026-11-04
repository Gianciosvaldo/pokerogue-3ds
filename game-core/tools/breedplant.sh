#!/bin/sh
# Uncommon Breed: show the comparator (and the host test) can fail. Replays one trace (from tools/breeddiff.sh) through copies of game-core, each with one line of src/gc_breed.c (or gc_mystery.c, gc_fof.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_breed_diff_test.c exiting 1, or, for what the oracle does not play (the battle: no fleeing, the party-full prompt, the berry items versus the Berry Pouch), by tests/mystery_breed_test.c failing.
# Not planted: the post-summon handler in battle-core (its stat stages are compared through the enemy's stages, its text through the battle events), the sprite, the moveset push for a Pokemon that knows fewer than four moves (every generated wild moveset has four), and the ignoreOverride-style equivalent mutants.
# Usage: tools/breedplant.sh <trace>   (BREED_PLANT_FROM=<name> starts at that plant, BREED_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-breed-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_breed.c}
  [ "$NAME" != "${BREED_PLANT_FROM:-}" ] || started=1
  [ -z "${BREED_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${BREED_PLANT_ONLY:-}" ] || case " $BREED_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_breed_diff_test" "$W/build/mystery_breed_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_breed_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_breed_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}


plant level-minus-one 's|    b->level = highest_level(run) - 2;|    b->level = highest_level(run) - 1;|'
plant level-first-member 's|    for (int k = 0; k < run->n_party; k++) if (run->party\[k\].b.level > hi) hi = run->party\[k\].b.level;|    for (int k = 0; k < 1; k++) if (run->party[k].b.level > hi) hi = run->party[k].b.level;|'
plant level-rolled 's|    return roll_boss(run, stream, boss, \&level, true);|    return roll_boss(run, stream, boss, \&level, false);|' gc_fof.c
plant egg-index-range '/^static bool breed_init/,/^}/ s|pr_rng_seed_int(\&r, 4, 0)|pr_rng_seed_int(\&r, 3, 0)|'
plant egg-slot '/^static bool breed_init/,/^}/ s|put_move(\&b->mon, b->egg_move, 0)|put_move(\&b->mon, b->egg_move, 1)|'
plant egg-not-put 's|        if (b->egg_move) put_move(\&b->mon, b->egg_move, 0);|        (void)b;|'
plant food-threshold 's|return berry_stacks_held(run) >= 4;|return berry_stacks_held(run) >= 3;|'
plant food-counts-pouch 's|if (!strcmp(run->items\[k\].key, "BERRY")) n += run->items\[k\].stack;|if (strstr(run->items[k].key, "BERRY")) n += run->items[k].stack;|'
plant food-option-always-on '/GC_ME_UNCOMMON_BREED) {/,/} else if/ s|        s->option_ok\[1\] = gc_breed_food_ok(run);|        s->option_ok[1] = true;|' gc_mystery.c
plant charm-option-always-on '/GC_ME_UNCOMMON_BREED) {/,/} else if/ s|        s->option_ok\[2\] = gc_mystery_primary_pokemon(run, type, 2) >= 0;|        s->option_ok[2] = true;|' gc_mystery.c
plant charm-primary-lost 's|    if (type == GC_ME_PART_TIMER \|\| type == GC_ME_UNCOMMON_BREED) {|    if (type == GC_ME_PART_TIMER) {|' gc_mystery.c
plant token-primary-name '/case GC_ME_UNCOMMON_BREED: {/,/break;/ s|"option3PrimaryName"|"option2PrimaryName"|' gc_mystery.c
plant token-enemy-lost 's|        if (gc_breed_tokens(run, \&enemy)) screen_arg(s, enemy);|        (void)enemy;|' gc_mystery.c
plant fight-reward-active 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant fight-reward-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant fight-egg-effect-lost 's|    if (b->egg_move) {|    if (0) {|'
plant fight-egg-effect-target 's|gc_me_add_start_effect(run, 2, self ? 2 : 0, b->egg_move);|gc_me_add_start_effect(run, 2, self ? 0 : 0, b->egg_move);|'
plant fight-egg-effect-source 's|gc_me_add_start_effect(run, 2, self ? 2 : 0, b->egg_move);|gc_me_add_start_effect(run, 3, self ? 2 : 0, b->egg_move);|'
plant fight-egg-effect-kind 's|const bool self = !strcmp(pr_moves\[b->egg_move\].kind, "SelfStatusMove");|const bool self = !strcmp(pr_moves[b->egg_move].kind, "StatusMove");|'
plant fight-post-summon-boundary 's|run->wave < 50 ? PR_ME_POST_BREED_LOW : PR_ME_POST_BREED_HIGH|run->wave < 60 ? PR_ME_POST_BREED_LOW : PR_ME_POST_BREED_HIGH|'
plant fight-post-summon-swapped 's|run->wave < 50 ? PR_ME_POST_BREED_LOW : PR_ME_POST_BREED_HIGH|run->wave < 50 ? PR_ME_POST_BREED_HIGH : PR_ME_POST_BREED_LOW|'
plant fight-boss 's|    e->b.is_boss = is_boss;|    e->b.is_boss = true;|' gc_fof.c
plant fight-boss-bars 's|        e->b.boss_segments = e->b.boss_segment_index = 0;|        e->b.boss_segments = 2;|' gc_fof.c
plant fight-flee-allowed 's|    in->me_no_flee = true;|    in->me_no_flee = false;|' gc_fof.c
plant food-stack-two 's|        it->stack--;|        it->stack -= 2;|'
plant food-three-berries 's|    for (int i = 0; i < 4 \&\& n > 0; i++) {|    for (int i = 0; i < 3 \&\& n > 0; i++) {|'
plant food-emptied-kept 's|run->items\[k\].stack <= 0) gc_item_remove_at(run, k);|run->items[k].stack <= -1) gc_item_remove_at(run, k);|'
plant food-list-not-shrunk 's|            for (int j = pick; j < n - 1; j++) idx\[j\] = idx\[j + 1\];|            (void)pick;|'
plant food-pick-range 's|        const int pick = pr_rng_seed_int(\&r, n, 0);|        const int pick = pr_rng_seed_int(\&r, n > 1 ? n - 1 : n, 0);|'
plant food-egg-stream 's|    extra_egg_move(\&b->mon, b->egg_move, \&after);|    extra_egg_move(\&b->mon, b->egg_move, \&r);|'
plant food-run-rng-lost 's|    run->run.rng = after;|    (void)after;|'
plant extra-egg-repeat 's|    } while (move == previous);|    } while (0);|'
plant extra-egg-slot 's|    if (move) put_move(m, move, 1);|    if (move) put_move(m, move, 0);|'
plant charm-iv-range 's|        const int v = pr_rng_seed_int(\&r, 31, 0);|        const int v = pr_rng_seed_int(\&r, 32, 0);|'
plant charm-iv-lower 's|        if (v > b->mon.b.ivs\[k\]) b->mon.b.ivs\[k\] = (uint8_t)v;|        b->mon.b.ivs[k] = (uint8_t)v;|'
plant charm-no-ivs 's|        if (v > b->mon.b.ivs\[k\]) b->mon.b.ivs\[k\] = (uint8_t)v;|        (void)v;|'
plant charm-exp-lost 's|    if (primary >= 0) gc_me_exp_value|    if (0) gc_me_exp_value|'
plant charm-exp-wave 's|base_exp \* (double)b->mon.b.level / 5 + 1, false);|base_exp * (double)b->mon.b.level / 5 + 1, true);|'
plant charm-exp-amount 's|base_exp \* (double)b->mon.b.level / 5 + 1, false);|base_exp * (double)b->mon.b.level / 5, false);|'
plant catch-lost 's|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));|    (void)run;|'
plant catch-obtained-text 's|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));|    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 1));|'
plant catch-enemy-items-kept 's|    run->n_enemy_items = 0;|    (void)run;|'
plant catch-ball 's|    gc_int(run)->thrown_ball = POKEBALL_TYPE_POKEBALL;|    gc_int(run)->thrown_ball = POKEBALL_TYPE_GREAT_BALL;|'
plant shop-no-fill 's|    c->fill_remaining = true;|    c->fill_remaining = false;|'
plant shop-lost '/^static void shop_and_leave/,/^}/ s|    gc_me_end_shop(run);|    gc_me_leave(run);|'
plant unported-species-guard 's|    if (!breed_init(run, \&b)) {|    if (!breed_init(run, \&b) \&\& 0) {|'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
