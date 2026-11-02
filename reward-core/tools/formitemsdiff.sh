#!/bin/sh
# Mega Stones, Max Mushrooms and the other form change items against upstream: generate N upstream reward screens for parties of
# Mega / Gigantamax capable species that always own the Mega Bracelet and the Dynamax Band (rewards.producer.test.ts with
# DIFF_FOCUS=formitems), replay them through reward-core, and print how many screens offered a form change item.
# Usage: tools/formitemsdiff.sh <upstream dir> [N] [seed]. Needs Node 22; the upstream checkout is the one
# battle-core/tools/setup-upstream.sh prepares. Exits with the comparator's status.
set -e
UP=${1:?upstream dir}; N=${2:-1500}; SEED=${3:-777}
HERE=$(cd "$(dirname "$0")/.." && pwd)
UP=$(CDPATH= cd -- "$UP" && pwd)
BUILD=${BUILD:-/tmp/reward-core-build}
case "$(node --version)" in v22.*) ;; *) echo "Node 22 required, found $(node --version)" >&2; exit 2 ;; esac
echo "node $(node --version)"
mkdir -p "$BUILD" "$UP/test/tests/zz-dump"
make -s -C "$HERE" BUILD="$BUILD"
cp "$HERE/tests/rewards.producer.test.ts" "$UP/test/tests/zz-dump/"
TRACE="$BUILD/form-items-$SEED.txt"
rm -f "$TRACE"
(cd "$UP" && DIFF_FOCUS=formitems DIFF_SEED=$SEED DIFF_N=$N DIFF_OUT="$TRACE" npx vitest run --no-cache test/tests/zz-dump/rewards.producer.test.ts >"$BUILD/form-items-$SEED.log" 2>&1) \
  || { echo "producer failed (log: $BUILD/form-items-$SEED.log)" >&2; exit 2; }
# coverage, from upstream's own screens: O <count> <n> then n x (key arg tier upgrades name)
awk '$1=="O"{ s++; n=$3; seen_f=0; for(i=0;i<n;i++){ k=$(4+i*5); a=$(5+i*5); nm=$(8+i*5);
        if(k=="FORM_CHANGE_ITEM"||k=="RARE_FORM_CHANGE_ITEM"){ f++; seen_f=1; if(nm ~ /^Max_Mushroom/) mm++; else if (nm ~ /ite/ ) mega++; else other++ } }
        if(seen_f) sf++ }
     END{ printf "upstream screens=%d with-form-item-screens=%d form-item-options=%d max-mushroom=%d mega-stone-like=%d other=%d\n", s, sf, f, mm, mega, other }' "$TRACE"
"$BUILD/rewardstest" "$TRACE"
