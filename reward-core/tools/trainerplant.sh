#!/bin/sh
# Card 97: show the trainer reward comparator can fail. Replays one trace (from tools/trainerdiff.sh) through six
# copies of reward-core, each with one line of src/trainer_rewards.c changed, and prints each one's totals. Every
# copy must exit 1. Usage: tools/trainerplant.sh <trace>
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$(cd "$HERE/.." && pwd)
WORK=${WORK:-/tmp/reward-core-plant}
fails=0
plant() {
  W="$WORK/$1"
  rm -rf "$W"; mkdir -p "$W"
  cp -r "$HERE" "$W/reward-core"
  ln -s "$ROOT/battle-core" "$W/battle-core"; ln -s "$ROOT/run-core" "$W/run-core"
  sed -i "$2" "$W/reward-core/src/trainer_rewards.c"
  n=$(diff "$HERE/src/trainer_rewards.c" "$W/reward-core/src/trainer_rewards.c" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$1: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$W/reward-core" BUILD="$W/build" >/dev/null
  st=0; "$W/build/trainerrewardstest" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$1: exit $st, $(grep '^cases' "$W/out.txt")"
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant voucher-unlock-inverted 's/const bool validated = !voucher_unlocked;/const bool validated = voucher_unlocked;/'
plant passive-ignored 's/return m->ability == ability || (m->passive \&\& m->passive == ability);/return m->ability == ability;/'
plant champion-draws-5 's/case CLASSIC_FIXED_BOSS_WAVES_CHAMPION: picks = 6;/case CLASSIC_FIXED_BOSS_WAVES_CHAMPION: picks = 5;/'
plant rival-bird-any-species 's/if (party\[1\].species == SPECIES_ID_SWELLOW \&\& has_ability(&party\[1\], ABILITY_ID_GUTS))/if (has_ability(\&party[1], ABILITY_ID_GUTS))/'
plant luck-upgrades-always 's/out->allow_luck_upgrades = r->allow_luck_upgrades;/out->allow_luck_upgrades = true;/'
plant money-multiplier-1 's/out->money_multiplier = t->money_multiplier;/out->money_multiplier = 1;/'
[ "$fails" = 0 ] || { echo "$fails planted bug(s) not caught"; exit 1; }
echo "all six planted bugs caught"
