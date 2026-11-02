#!/bin/sh
# Show the form change item comparator can fail. Replays one trace (from tools/formitemsdiff.sh) through copies of reward-core,
# each with one line of src/reward.c or src/pools.c changed, and prints each one's totals. Every copy must exit 1.
# Usage: tools/formitemsplant.sh <trace>
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(cd "$(dirname "$0")/.." && pwd)
ROOT=$(cd "$HERE/.." && pwd)
WORK=${WORK:-/tmp/reward-core-plant-formitems}
fails=0; total=0
plant() {
  W="$WORK/$1"; F=$2; total=$((total + 1))
  rm -rf "$W"; mkdir -p "$W"
  cp -r "$HERE" "$W/reward-core"
  ln -s "$ROOT/battle-core" "$W/battle-core"; ln -s "$ROOT/run-core" "$W/run-core"
  sed -i "$3" "$W/reward-core/src/$F"
  n=$(diff "$HERE/src/$F" "$W/reward-core/src/$F" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$1: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$W/reward-core" BUILD="$W/build" >/dev/null
  st=0; "$W/build/rewardstest" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  echo "$1: exit $st, $(grep '^cases' "$W/out.txt")"
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant mega-needs-no-bracelet reward.c 's/if ((fc->flags \& 1) \&\& !mega) continue;/if ((fc->flags \& 1) \&\& 0) continue;/'
plant gmax-needs-no-band reward.c 's/if ((fc->flags \& 2) \&\& !gmax) continue;/if ((fc->flags \& 2) \&\& 0) continue;/'
plant pre-form-key-ignored reward.c 's/if (fc->pre_form_key != key) continue;/if (0) continue;/'
plant inactive-triggers-allowed reward.c 's/if (!(fc->flags \& 4)) continue;   \/\* t.active \*\//if (0) continue;/'
plant rare-split-inverted reward.c 's/if ((it \&\& it < 150) == rare) pool\[n++\]/if ((it \&\& it < 150) != rare) pool[n++]/'
plant form-weight-5 pools.c 's/return imin(ceil_div(wave, 50), 4) \* 6;/return imin(ceil_div(wave, 50), 4) * 5;/'
plant bracelet-weight-8 pools.c 's/return imin(ceil_div(wave, 50), 4) \* 9;/return imin(ceil_div(wave, 50), 4) * 8;/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
