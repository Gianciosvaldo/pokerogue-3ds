#!/bin/sh
# The Challenge Mode battle rules (Inverse Battle, Flip Stat, Passives) and what the rewards read of the challenges: show the comparator can fail. Replays one trace (from
# tools/challengestartersdiff.sh) through copies of game-core and battle-core, each with one line of battle-core or game-core changed, and prints
# each one's totals. Every copy must exit 1.
# Usage: tools/challengebattleplant.sh <trace>
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-challenge-battle-plant}
fails=0; total=0
plant() {
  NAME=$1; FILE=$2; EXPR=$3
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W"
  # copies, not links: run-core and ai-core build battle-core from their own ../battle-core, which a link would resolve to the unchanged one
  for d in game-core battle-core run-core ai-core reward-core; do mkdir -p "$W/$d"; (cd "$ROOT/$d" && tar cf - --exclude=./build --exclude=./node_modules .) | tar xf - -C "$W/$d"; done
  mkdir -p "$W/build"
  sed -i "$EXPR" "$W/$FILE"
  n=$(diff "$ROOT/$FILE" "$W/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j8 -C "$W/game-core" BUILD="$W/build" "$W/build/challenge_starters_diff_test" >"$W/build.txt" 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/challenge_starters_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt")"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W/build"
}
# Inverse Battle: getTypeDamageMultiplier
plant inverse-off battle-core/src/damage.c 's/if (bt \&\& (bt->challenge_flags \& (1u << CHALLENGES_INVERSE_BATTLE))) {/if (0) {/'
plant inverse-keeps-immunity battle-core/src/damage.c 's/        if (m < 1) m = 2;/        if (m < 1 \&\& m > 0) m = 2;/'
plant inverse-keeps-super battle-core/src/damage.c 's/        else if (m > 1) m = 0.5;/        else if (m > 2) m = 0.5;/'
# Flip Stat: calculateBaseStats
plant flip-off battle-core/src/pokemon.c 's/const bool flip = (bt->challenge_flags \& (1u << CHALLENGES_FLIP_STAT)) != 0;/const bool flip = false;/'
plant flip-wrong-pairs battle-core/src/pokemon.c 's/base\[s\] = f->base_stats\[flip ? 5 - s : s\];/base[s] = f->base_stats[flip ? (s + 3) % 6 : s];/'
plant flip-not-on-the-fusion battle-core/src/pokemon.c 's/if (ff) fused\[s\] = ff->base_stats\[flip ? 5 - s : s\];/if (ff) fused[s] = ff->base_stats[s];/'
plant flip-not-on-the-run-battle game-core/src/gc_challenge.c 's/run->bt.challenge_flags = r->mode == GAME_MODES_CHALLENGE ? r->challenge_flags : 0;/run->bt.challenge_flags = 0;/'
# Passives: hasPassive and applyPassiveAccess
plant passives-no-trainer battle-core/src/pokemon.c 's/const bool trainer = p->has_trainer \&\& !p->is_player;/const bool trainer = false;/'
plant passives-all-is-trainers battle-core/src/pokemon.c 's/bt->challenge_passives != 1) has = true;/bt->challenge_passives > 2) has = true;/'
plant passives-boss-wave-ignores-challenge battle-core/src/pokemon.c 's/if (!p->is_player \&\& !passives \&\& ((classic_like/if (!p->is_player \&\& ((classic_like/'
plant challenge-final-boss-not-classic battle-core/src/pokemon.c 's/const bool classic_like = gm == GAME_MODES_CLASSIC || gm == GAME_MODES_CHALLENGE;/const bool classic_like = gm == GAME_MODES_CLASSIC;/'
# The rewards: game-core's reward context carries the run's challenges, and reward-core recalculates with Flip Stat
plant rewards-without-challenges game-core/src/gc_reward.c 's/c->challenges = run->run.mode == GAME_MODES_CHALLENGE ? run->run.challenge_flags : 0;/c->challenges = 0;/'
plant rewards-stats-without-flip reward-core/src/pools.c 's/if (!(ctx->challenges \& (1u << CHALLENGES_FLIP_STAT))) return;/return;/'
plant rewards-fresh-pools-ignored reward-core/src/pools.c 's/(!has_challenge(ctx, CHALLENGES_FRESH_START) \&\& ctx->unlocked_eviolite)/(ctx->unlocked_eviolite)/'
# The run rules (PR 3): Limited Support's shop and heal, Hardcore's shop, revive and Revival Blessing, Limited Catch's party add
plant shop-hardcore-keeps-revives run-core/src/shop.c 's/if (hardcore \&\& (table\[i\].item == PR_SHOP_REVIVE/if (0 \&\& (table[i].item == PR_SHOP_REVIVE/'
plant shop-hardcore-keeps-sacred-ash run-core/src/shop.c 's/ || table\[i\].item == PR_SHOP_SACRED_ASH)) continue;/)) continue;/'
plant shop-hardcore-outside-challenge run-core/src/shop.c 's/const bool hardcore = run->mode == GAME_MODES_CHALLENGE \&\& (run->challenge_flags \& PR_CHALLENGE_HARDCORE);/const bool hardcore = false;/'
plant support-shop-open run-core/src/shop.c 's/status = run->challenge_limited_support == 1;/status = true;/'
plant support-heal-on-both run-core/src/shop.c 's/return run->challenge_limited_support == 2;/return run->challenge_limited_support != 1;/'
plant prevent-revive-off run-core/src/shop.c 's/    return run->mode == GAME_MODES_CHALLENGE \&\& (run->challenge_flags \& PR_CHALLENGE_HARDCORE);/    return false;/'
plant support-not-synced game-core/src/gc_challenge.c 's/r->challenge_limited_support = set->value\[CHALLENGES_LIMITED_SUPPORT\];/r->challenge_limited_support = 0;/'
plant revival-blessing-allowed battle-core/src/damage.c 's/move == MOVE_ID_REVIVAL_BLESSING);/move == MOVE_ID_NONE);/'
plant usable-skips-challenge battle-core/src/commands.c 's/if (p->is_player \&\& !pr_challenge_move_allowed(bt, move)) {/if (0) {/'
plant copycat-skips-challenge battle-core/src/called_moves.c 's/return !pr_moves\[move\].unimplemented \&\& pr_challenge_move_allowed(bt, move);/return !pr_moves[move].unimplemented;/'
plant limited-catch-any-wave game-core/src/gc_challenge.c 's/|| run->wave % 10 == 1;/|| run->wave % 10 != 0;/'
plant limited-catch-no-teleport game-core/src/gc_challenge.c 's/return gc_int((gc_run \*)run)->me_teleport_catch || run->wave/return run->wave/'
echo "challenge battle plants: $total run, $fails not caught"
[ "$fails" = 0 ]
