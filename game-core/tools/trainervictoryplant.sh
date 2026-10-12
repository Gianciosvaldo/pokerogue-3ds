#!/bin/sh
# Trainer victory: show the comparator can fail. Replays one trace (from tools/trainervictorydiff.sh) through copies of
# game-core, each with one line of src/gc_phases.c, src/gc_stubs.c or src/gc_reward.c changed, and prints each one's totals. Every copy
# must exit 1. The first one is the bug game-core had before: the voucher reward on the first win instead of later ones.
# Usage: GC_LOCALES=<upstream dir>/locales/en tools/trainervictoryplant.sh <trace>
set -e
: "${GC_LOCALES:?GC_LOCALES must name upstream's locales/en folder}"
export GC_LOCALES
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-plant}
fails=0
plant() {
  NAME=$1; FILE=$2; EXPR=$3
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/trainer_victory_diff_test" >/dev/null 2>&1
  st=0; "$W/build/trainer_victory_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep '^cases' "$W/out.txt")"
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant unlock-inverted gc_phases.c 's/pr_reward_trainer_victory_rewards(run->trainer_type, unlocked, false, &tv)/pr_reward_trainer_victory_rewards(run->trainer_type, !unlocked, false, \&tv)/'
plant event-upgrade-on gc_phases.c 's/pr_reward_trainer_victory_rewards(run->trainer_type, unlocked, false, &tv)/pr_reward_trainer_victory_rewards(run->trainer_type, unlocked, true, \&tv)/'
plant reward-key-always-plus gc_phases.c 's/vp.key = pr_item_keys\[tv.items\[tv.n_items - 1\]\];/vp.key = "VOUCHER_PLUS";/'
plant unlock-counts-plus gc_phases.c 's/gc_system_unlock_voucher(run->sys, run->trainer_type, tv.voucher_type);/gc_system_unlock_voucher(run->sys, run->trainer_type, GC_VOUCHER_PLUS);/'
plant reward-counts-wrong-type gc_stubs.c 's/gc_system_add_voucher_count(run->sys, GC_VOUCHER_REGULAR, 1)/gc_system_add_voucher_count(run->sys, GC_VOUCHER_PLUS, 1)/'
plant reward-counts-nothing gc_stubs.c 's/\(VOUCHER_PLUS")) \)gc_system_add_voucher_count(run->sys, GC_VOUCHER_PLUS, 1);/\1(void)0;/'
plant reward-funcs-not-queued gc_phases.c 's/for (int i = 0; i < n_funcs \&\& i < tv.n_items; i++) {/for (int i = 0; i < 0 \&\& i < tv.n_items; i++) {/'
plant reward-funcs-first-item gc_phases.c 's/rp.key = pr_item_keys\[tv.items\[i\]\];/rp.key = pr_item_keys[tv.items[0]];/'
plant stack-cap-ignored gc_stubs.c 's/gc_item_stack(run, key) + 1 > pr_items\[idx\].max_stack)/gc_item_stack(run, key) + 1 > 999)/'
plant access-item-capped gc_stubs.c 's/idx != PR_ITEM_TERA_ORB;/1;/'
plant full-gives-nothing gc_stubs.c 's/^        gc_port_give_reward(run, pr_item_keys\[fallback\]);/        (void)fallback;/'
plant full-wrong-tier gc_stubs.c 's/pr_player_pool\[0\].start/pr_player_pool[1].start/'
plant full-message-dropped gc_reward.c 's/^    if (instead >= 0)   \/\*/    if (0 \&\& instead >= 0)   \/*/'
plant money-multiplier-1 gc_phases.c 's/p.d = c ? c->money_multiplier : 1;   \/\* config.moneyMultiplier \*\//p.d = 1;/'
[ "$fails" = 0 ] || { echo "$fails planted bug(s) not caught"; exit 1; }
echo "all fourteen planted bugs caught"
