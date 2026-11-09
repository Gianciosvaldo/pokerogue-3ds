#!/bin/sh
# The Safari Zone: show the comparator (and the host test) can fail. Replays one trace (from tools/safaridiff.sh) through copies of
# game-core, each with one line of src/gc_safari.c (or another file the port touches) changed, and prints each one's totals. Every copy must be caught: by
# tests/mystery_safari_diff_test.c exiting 1, or, for what the oracle does not play (the catch itself: the party, the ball, the "caught" text, the dex, the entry fee refused
# without money), by tests/mystery_safari_test.c failing.
# Not planted: the ball's twitch roll equal to the rate exactly (< against <=: one roll in 65536, so a few thousand rolls never meet it), the Pokemon tiers above the highest starter cost of the list (6+) and the legendary flag (every legendary is an excluded paradox or has cost 6+ or 0, so (0,5)/(0,6) and the flag are equivalent), the clamp of the wallet (the fee never exceeds what the player has: an equivalent change), the Pokemon's and the trainer's animations and sounds (the app's).
# Usage: tools/safariplant.sh <trace>   (SAFARI_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-safari-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_safari.c}
  [ "$NAME" != "${SAFARI_PLANT_FROM:-}" ] || started=1
  [ -z "${SAFARI_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_safari_diff_test" "$W/build/mystery_safari_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_safari_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_safari_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
plant fee-multiplier 's/#define SAFARI_MONEY_MULTIPLIER 2.0/#define SAFARI_MONEY_MULTIPLIER 1.5/'
plant fee-unpaid 's/^    pay(run, gc_safari_cost(run));/    (void)0;/'
plant fee-message-missing 's/^    gc_ev_msg(run, "mysteryEncounterMessages:paidMoney", 1, pr_arg_int("amount", (int32_t)cost));/    (void)cost;/'
plant three-pokemon 's/#define NUM_SAFARI_ENCOUNTERS 3/#define NUM_SAFARI_ENCOUNTERS 2/'
plant summon-stream 's/run->wave \* 1000 \* in->safari.remaining, NULL);/run->wave * 100 * in->safari.remaining, NULL);/'
plant summon-stream-after-decrement 's/run->wave \* 1000 \* in->safari.remaining, NULL);/run->wave * 1000 * (in->safari.remaining + 1), NULL);/'
plant remaining-message-count 's/1, pr_arg_int("remainingCount", in->safari.remaining));/1, pr_arg_int("remainingCount", in->safari.remaining - 1));/'
plant level-of-wave 's/pr_run_get_level_for_wave(&run->run, &r, run->wave);/pr_run_get_level_for_wave(\&run->run, \&r, run->wave + 1);/'
plant tier-max 's/gc_species_by_starter_cost(&r, 0, 5, paradox/gc_species_by_starter_cost(\&r, 0, 4, paradox/'
plant tier-min 's/gc_species_by_starter_cost(&r, 0, 5, paradox/gc_species_by_starter_cost(\&r, 2, 5, paradox/'
plant allow-sub-legendary 's/NULL, 0, false, false, false);/NULL, 0, true, false, false);/'
plant allow-mythical 's/NULL, 0, false, false, false);/NULL, 0, false, false, true);/'
plant paradox-list-dropped 's/(int)(sizeof paradox \/ sizeof paradox\[0\])/0/'
plant shiny-threshold 's/gc_try_set_shiny_seed(run, &r, e, 64, 0);/gc_try_set_shiny_seed(run, \&r, e, 128, 0);/'
plant shiny-reroll-missing 's/^    gc_try_set_shiny_seed(run, &r, e, 64, 0);/    (void)0;/'
plant hidden-threshold 's/gc_try_reroll_hidden_ability(run, &r, e, 256);/gc_try_reroll_hidden_ability(run, \&r, e, 128);/'
plant hidden-reroll-missing 's/^    gc_try_reroll_hidden_ability(run, &r, e, 256);/    (void)0;/'
plant shiny-charm-factor 's/if (charms > 0) threshold \*= pow(2, 1 + charms);/if (charms > 0) threshold *= pow(2, charms);/' gc_gts.c
plant hidden-charm-factor 's/if (stacks > 0) threshold \*= pow(2, -1 - stacks);/if (stacks > 0) threshold *= pow(2, -stacks);/' gc_gts.c
plant shuffle-index 's/const int j = (int)floor(pr_rng_frac(r) \* (i + 1));/const int j = (int)floor(pr_rng_frac(r) * i);/' gc_darkdeal.c
plant seen-unmarked 's/if (run->sys) gc_system_set_pokemon_seen(run->sys, e, true, false);/(void)0;/'
plant stages-kept 's/in->safari.catch_stage = in->safari.flee_stage = 0;/in->safari.catch_stage = 0;/'
plant ball-rate 's/\* 1.5 \* stage_modifier(in->safari.catch_stage)/* 1.0 * stage_modifier(in->safari.catch_stage)/'
plant ball-twitch-constant 's/1048560.0 \/ sqrt(sqrt(16711680.0 \/ catch_rate))/1048000.0 \/ sqrt(sqrt(16711680.0 \/ catch_rate))/'
plant ball-two-shakes 's/for (int shake = 0; shake < 3 \&\& caught; shake++)/for (int shake = 0; shake < 2 \&\& caught; shake++)/'
plant ball-rolls-all 's/for (int shake = 0; shake < 3 \&\& caught; shake++)/for (int shake = 0; shake < 3; shake++)/'
plant catch-stage-up-clamp 's/const int up = stage < 0 ? 0 : stage > 6 ? 6 : stage;/const int up = stage < 0 ? 0 : stage > 5 ? 5 : stage;/'
plant catch-stage-down-clamp 's/const int down = stage > 0 ? 0 : stage < -6 ? -6 : stage;/const int down = stage > 0 ? 0 : stage < -5 ? -5 : stage;/'
plant stage-change-clamp-high 's/\*stage = (int8_t)(v < -6 ? -6 : v > 6 ? 6 : v);/*stage = (int8_t)(v < -6 ? -6 : v > 5 ? 5 : v);/'
plant stage-change-clamp-low 's/\*stage = (int8_t)(v < -6 ? -6 : v > 6 ? 6 : v);/*stage = (int8_t)(v < -5 ? -5 : v > 6 ? 6 : v);/'
plant flee-formula 's/(255.0 \* 255.0 - rate \* rate) \/ 255.0 \/ 2.0)/(255.0 * 255.0 - rate * rate) \/ 255.0 \/ 3.0)/'
plant flee-roll-range 's/const int flee_roll = roll(run, 256);/const int flee_roll = roll(run, 255);/'
plant flee-roll-inclusive 's/if (flee_roll < flee_rate) {/if (flee_roll <= flee_rate) {/'
plant ball-roll-range 's/caught = roll(run, 65536) < twitch;/caught = roll(run, 65535) < twitch;/'
plant chance-roll-range 's/if (chance \&\& roll(run, 10) >= chance) return false;/if (chance \&\& roll(run, 9) >= chance) return false;/'
plant chance-roll-inclusive 's/if (chance \&\& roll(run, 10) >= chance) return false;/if (chance \&\& roll(run, 10) > chance) return false;/'
plant flee-uses-catch-stage 's/stage_modifier(in->safari.flee_stage);/stage_modifier(in->safari.catch_stage);/'
plant flee-skipped-after-mud 's/^    return end_turn(run, ACT_MUD);/    return gc_safari_next(run, ACT_MUD);/'
plant bait-catch-stage 's/change_stage(run, &in->safari.catch_stage, 2, 0);/change_stage(run, \&in->safari.catch_stage, 1, 0);/'
plant bait-flee-chance 's/change_stage(run, &in->safari.flee_stage, 1, 8);/change_stage(run, \&in->safari.flee_stage, 1, 7);/'
plant bait-flee-change 's/change_stage(run, &in->safari.flee_stage, 1, 8);/change_stage(run, \&in->safari.flee_stage, 2, 8);/'
plant bait-text-swapped 's/more ? NS ":safari.eating" : NS ":safari.busyEating"/more ? NS ":safari.busyEating" : NS ":safari.eating"/'
plant bait-cursor 's/^    return end_turn(run, ACT_BAIT);/    return end_turn(run, ACT_BALL);/'
plant mud-flee-stage 's/change_stage(run, &in->safari.flee_stage, -2, 0);/change_stage(run, \&in->safari.flee_stage, -1, 0);/'
plant mud-catch-chance 's/change_stage(run, &in->safari.catch_stage, -1, 8);/change_stage(run, \&in->safari.catch_stage, -1, 9);/'
plant mud-catch-change 's/change_stage(run, &in->safari.catch_stage, -1, 8);/change_stage(run, \&in->safari.catch_stage, -2, 8);/'
plant mud-text-swapped 's/angrier ? NS ":safari.angry" : NS ":safari.besideItselfAngry"/angrier ? NS ":safari.besideItselfAngry" : NS ":safari.angry"/'
plant mud-cursor 's/^    return end_turn(run, ACT_MUD);/    return end_turn(run, ACT_BAIT);/'
plant ball-cursor 's/^        return end_turn(run, ACT_BALL);/        return end_turn(run, ACT_BAIT);/'
plant run-cursor 's/^    return gc_safari_next(run, ACT_RUN);/    return gc_safari_next(run, 0);/'
plant catch-cursor 's/gc_rq_unshift(run, gc_rp(RP_SAFARI, ACT_BALL, 0, 0));/gc_rq_unshift(run, gc_rp(RP_SAFARI, ACT_BAIT, 0, 0));/'
plant broke-free-missing 's/^        gc_ev_msg(run, "battle:pokemonBrokeFree", 1, gc_m_mon_name(run, "pokemonName", 1, 0));/        (void)0;/'
plant pokemon-fled-missing 's/^        gc_ev_msg(run, "battle:pokemonFled", 1, gc_m_mon_name(run, "pokemonName", 1, 0));/        (void)0;/'
plant player-fled-missing 's/^    gc_ev_msg(run, "battle:playerFled", 1, gc_m_mon_name(run, "pokemonName", 1, 0));/    (void)0;/'
plant watching-missing 's/^    gc_ev_msg(run, NS ":safari.watching", 1, gc_m_mon(run, "pokemonName", 1, 0));/    (void)0;/'
plant catch-not-queued 's/^    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 3));/    (void)0;/'
plant catch-wrong-ball 's/in->thrown_ball = POKEBALL_TYPE_POKEBALL;/in->thrown_ball = POKEBALL_TYPE_GREAT_BALL;/'
plant caught-text-missing 's/^    if (p->c == 3) gc_ev_msg(run, "battle:pokemonCaught", 1, gc_m_mon_name(run, "pokemonName", 1, p->a));/    (void)0;/' gc_catch.c
plant leave-without-shop 's/^    gc_me_leave_with_heal_shop(run);/    (void)0;/'
plant minigame-never-ends 's/^    gc_int(run)->safari.active = false;/    (void)0;/'
plant no-way-out 's/if (in->safari.remaining <= 0) {/if (in->safari.remaining < 0) {/'
plant entry-gate-open 's/s->option_ok\[0\] = run->money >= gc_safari_cost(run);/s->option_ok[0] = true;/' gc_mystery.c
plant entry-gate-exclusive 's/s->option_ok\[0\] = run->money >= gc_safari_cost(run);/s->option_ok[0] = run->money > gc_safari_cost(run);/' gc_mystery.c
plant token-count 's/screen_arg(s, pr_arg_int("numEncounters", 3));/screen_arg(s, pr_arg_int("numEncounters", 4));/' gc_mystery.c
plant token-money 's/screen_arg(s, pr_arg_money_en_us("money", gc_safari_cost(run)));/screen_arg(s, pr_arg_money_en_us("money", gc_safari_cost(run) + 1));/' gc_mystery.c
echo "planted $total, not caught $fails"
[ "$fails" = 0 ]
