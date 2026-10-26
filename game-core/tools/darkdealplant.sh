#!/bin/sh
# Dark Deal: show the comparator can fail. Replays one trace (from tools/darkdealdiff.sh) through copies of game-core, each with one
# line of src/gc_darkdeal.c (or gc_encounter.c) changed, and prints each one's totals. Every copy must exit 1.
# Not planted: the top tier's upper end (the roll below 5 asks for cost 9 to 10; no trace reached a cost-10 starter, cause not investigated) and the form-change items that stay behind (the producer gives no Pokemon one, so no trace reaches that line).
# Usage: tools/darkdealplant.sh <trace>
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-darkdeal-plant}
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_darkdeal.c}
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_darkdeal_diff_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_darkdeal_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant roll-65 's/roll >= 65 ? 6/roll >= 66 ? 6/'
plant roll-15 's/roll >= 15 ? 7/roll >= 16 ? 7/'
plant roll-5 's/roll >= 5 ? 8 : 9;/roll >= 6 ? 8 : 9;/'
plant fainted-pick 's/return fainted\[pr_rng_seed_int(r, n_fainted, 0)\];/return fainted[0];/'
plant able-pick 's/if (n_able > 0) return able\[pr_rng_seed_int(r, n_able, 0)\];/if (n_able > 0) return able[0];/'
plant last-able 's/if (n_able == 1 \&\& n_fainted > 0)/if (n_able == 0 \&\& n_fainted > 0)/'
plant shuffle-step 's/const int j = (int)floor(pr_rng_frac(r) \* (i + 1));/const int j = (int)floor(pr_rng_frac(r) * i);/'
plant rogue-balls 's/gc_reward_gain(run, "ROGUE_BALL");/(void)0;/'
plant level-roll 's/const int level = pr_run_get_level_for_wave(\&run->run, \&r, run->wave);/const int level = run->wave;/'
plant excluded-mythical 's/SPECIES_ID_MEW, SPECIES_ID_CELEBI, SPECIES_ID_JIRACHI,/SPECIES_ID_MEW, SPECIES_ID_CELEBI,/'
plant single-battle 's/    run->double_battle = false;/    (void)0;/'
plant pool-thresholds 's/^        gc_enemy_pool_thresholds(run);/        (void)0;/' gc_encounter.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
