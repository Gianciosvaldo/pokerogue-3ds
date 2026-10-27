#!/bin/sh
# Berries Abound: show the comparator can fail. Replays one trace (from tools/berriesdiff.sh) through copies of game-core, each with one line
# of src/gc_berries.c (or gc_encounter.c, gc_mystery.c, gc_phases.c, gc_battle.c; or battle-core's src/switching.c and src/commands.c, for
# which battle-core and run-core are copied and rebuilt) changed, and prints each one's totals. Every copy must exit 1.
# A plant counts as caught when the comparator OR tests/mystery_berries_test.c (the host test of the battle itself) fails.
# Not planted: the enrage message's key (the comparator takes upstream's from the oracle, but battle-core's own is not read back), the order of the callback's message and the berries.
# Usage: tools/berriesplant.sh <trace>   (BRY_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-berries-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_berries.c}
  [ "$NAME" != "${BRY_PLANT_FROM:-}" ] || started=1
  [ -z "${BRY_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  case "$FILE" in
    battle-core/*)
      for d in battle-core run-core; do mkdir -p "$W/$d"; (cd "$ROOT/$d" && tar cf - --exclude=./build .) | tar xf - -C "$W/$d"; done
      for d in ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
      TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *)
      for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
      TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_berries_diff_test" "$W/build/mystery_berries_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_berries_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  ht=0; "$W/build/mystery_berries_test" >"$W/host.txt" 2>&1 || ht=$?
  echo "$NAME: comparator exit $st, host test exit $ht, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || [ "$ht" != 0 ] || fails=$((fails + 1))
}
plant count-threshold-40 's|wave > 40 ? 4 : 2|wave > 45 ? 4 : 2|'
plant count-threshold-120 's|wave > 120 ? 5|wave > 125 ? 5|'
plant count-threshold-160 's|wave > 160 ? 7|wave > 165 ? 7|'
plant fastest-tie 's|run->party\[best\].b.stats\[STAT_SPD\] < run->party\[idx\].b.stats\[STAT_SPD\]|run->party[best].b.stats[STAT_SPD] <= run->party[idx].b.stats[STAT_SPD]|'
plant fastest-fainted 's|if (!gc_mon_allowed(\&run->party\[idx\])) continue;|(void)0;|'
plant speed-factor 's|\* 1.1) : 0;|* 1.2) : 0;|'
plant grab-step 's|/ 0.08 + 0.5|/ 0.09 + 0.5|'
plant grab-minimum 's|return n > 2 ? n : 2;|return n > 3 ? n : 3;|'
plant grab-cap 's|rounded < b->n ? rounded : b->n;|rounded < b->n ? rounded : b->n + 1;|'
plant berry-cap 's|? 2 : 3;|? 2 : 2;|'
plant berry-roll-range 's|BERRY_TYPE_LEPPA + 1, 0)|BERRY_TYPE_LEPPA, 0)|'
plant berry-no-priority 's|if (prioritized >= 0) order\[n++\] = prioritized;|(void)0;|'
plant berry-full-check 's|held < berry_cap(type)|held <= berry_cap(type)|'
plant callback-stream 's|run->wave \* 1000, NULL|run->wave * 1001, NULL|'
plant callback-count 's|pr_arg_int("count", count)|pr_arg_int("count", count + 1)|'
plant shop-size '/static void roll_shop/,/^}/ s|i < 5|i < 4|'
plant fight-stream 's|fight(run, \&b, \&stream, shop, PR_ME_POST_NONE)|fight(run, \&b, NULL, shop, PR_ME_POST_NONE)|'
plant lost-race-stream 's|\&run->run.rng, shop|\&stream, shop|'
plant enrage-threshold 's|run->wave < 50 ? PR_ME_POST_ENRAGED_LOW|run->wave < 55 ? PR_ME_POST_ENRAGED_LOW|'
plant fight-fill 's|in->me_reward.fill = false;|in->me_reward.fill = true;|'
plant fight-no-callback 's|in->me_reward.before = true;|in->me_reward.before = false;|'
plant fight-no-berries 's|in->berries.pending = true;|in->berries.pending = false;|'
plant fight-berry-count 's|in->berries.n = (uint8_t)b->n;|in->berries.n = (uint8_t)(b->n + 1);|'
plant race-no-priority 's|give_berries(run, count, b.fastest);|give_berries(run, count, -1);|'
plant race-exp-species 's|gc_me_steal_exp(run, b.fastest, b.boss.b.species);|gc_me_steal_exp(run, b.fastest, 1);|'
plant race-shop-fill 's|c->fill_remaining = false;|c->fill_remaining = true;|'
plant leave-without-heal 's|if (option == 2) { gc_me_leave_with_heal_shop(run); return; }|if (option == 2) { gc_me_leave(run); return; }|'
plant boss-id-stream 's|4294967296.0|4294967295.0|' gc_fof.c
plant win-shop-short 's|i < in->me_reward.n_more \&\&|i < in->me_reward.n_more - 1 \&\&|' gc_mystery.c
plant win-callback-dropped 's|else gc_berries_reward(run);|(void)0;|' gc_phases.c
plant enrage-low-set 's|static const int32_t low\[3\] = {STAT_DEF, STAT_SPDEF, STAT_SPD};|static const int32_t low[3] = {STAT_DEF, STAT_SPDEF, STAT_ATK};|' battle-core/src/switching.c
plant enrage-high-set 's|{STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPD};|{STAT_ATK, STAT_DEF, STAT_ATK, STAT_SPDEF, STAT_SPD};|' battle-core/src/switching.c
plant enrage-stages 's|is_low ? low : high, is_low ? 3 : 5, 1, p);|is_low ? low : high, is_low ? 3 : 5, 2, p);|' battle-core/src/switching.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
