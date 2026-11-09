#!/bin/sh
# Show the selection-rules comparator (tests/mystery_rules_diff_test.c) can fail: replays the frozen upstream rules through copies of game-core, each with one line of
# src/gc_mystery.c changed (a rule of the table, a biome of the map), and prints each one's totals. Every copy must be caught.
# Usage: tools/rulesplant.sh [rules.txt]   (default tests/fixtures/mystery-rules.native.txt)
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
RULES=${1:-$HERE/tests/fixtures/mystery-rules.native.txt}
WORK=${WORK:-/tmp/game-core-rules-plant}
make -s -C "$HERE" build/mystery_rules_diff_test
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W"
  cp -a "$HERE" "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  ORIG="$HERE/src/gc_mystery.c"; TARGET="$W/game-core/src/gc_mystery.c"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -C "$W/game-core" BUILD="$W/game-core/build" BATTLE_CORE="$W/battle-core" "$W/game-core/build/mystery_rules_diff_test" >/dev/null 2>&1 || true
  st=0; "$W/game-core/build/mystery_rules_diff_test" "$RULES" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
BIOME='/^static bool biome_has_id/,/^}/'
plant chest-not-any-biome "${BIOME}s/ case GC_ME_MYSTERIOUS_CHEST://"
plant snorlax-only-plains "${BIOME}s/ || b == BIOME_ID_GRASS || b == BIOME_ID_TALL_GRASS;\$/;/"
plant snorlax-no-tall-grass "${BIOME}s/ || b == BIOME_ID_TALL_GRASS;\$/;/"
plant avarice-no-forest "${BIOME}s/ || b == BIOME_ID_TALL_GRASS || b == BIOME_ID_FOREST;\$/ || b == BIOME_ID_TALL_GRASS;/"
plant avarice-no-grass "${BIOME}s/return b == BIOME_ID_GRASS || b == BIOME_ID_TALL_GRASS || b == BIOME_ID_FOREST;/return b == BIOME_ID_TALL_GRASS || b == BIOME_ID_FOREST;/"
plant breeder-no-space "${BIOME}s/^    if (type == GC_ME_THE_EXPERT_POKEMON_BREEDER && b == BIOME_ID_SPACE) return true;.*/    ;/"
plant dance-no-island "${BIOME}s/ || b == BIOME_ID_RUINS || b == BIOME_ID_TEMPLE || b == BIOME_ID_ISLAND;/ || b == BIOME_ID_RUINS || b == BIOME_ID_TEMPLE;/"
plant safari-no-jungle "${BIOME}s/return b == BIOME_ID_FOREST || b == BIOME_ID_SWAMP || b == BIOME_ID_JUNGLE;/return b == BIOME_ID_FOREST || b == BIOME_ID_SWAMP;/"
plant superfan-extreme "${BIOME}s/if (type == GC_ME_BUG_TYPE_SUPERFAN \&\& non_extreme) return true;/if (type == GC_ME_BUG_TYPE_SUPERFAN \&\& human) return true;/"
plant civ-no-island "${BIOME}s/ || b == BIOME_ID_SLUM || b == BIOME_ID_ISLAND;/ || b == BIOME_ID_SLUM;/"
plant human-includes-space "/bool human = /,+1s/ \&\& b != BIOME_ID_ABYSS \&\& b != BIOME_ID_SPACE \&\& b != BIOME_ID_END;\$/ \&\& b != BIOME_ID_ABYSS \&\& b != BIOME_ID_END;/"
plant fiery-wrong-biome "${BIOME}s/return b == BIOME_ID_VOLCANO;/return b == BIOME_ID_DESERT;/"
plant cap-delibirdy-4 "/\[GC_ME_DELIBIRDY\]/s/10,180,1,0/10,180,4,0/"
plant cap-fight-or-flight-unbounded "/\[GC_ME_FIGHT_OR_FLIGHT\]/s/10,180,1,0/10,180,0,0/"
plant cap-dark-deal-unbounded "/\[GC_ME_DARK_DEAL\]/s/30,180,1,2/30,180,0,2/"
plant dept-store-to-180 "/\[GC_ME_DEPARTMENT_STORE_SALE\]/s/10,100,/10,180,/"
plant snorlax-to-180 "/\[GC_ME_SLUMBERING_SNORLAX\]/s/15,150,/15,180,/"
plant weird-dream-min-wave "/\[GC_ME_WEIRD_DREAM\]/s/30,140,/10,140,/"
plant clowning-tier "/\[GC_ME_CLOWNING_AROUND\]/s/GC_ME_ULTRA, 80/GC_ME_GREAT, 80/"
plant chest-party "/\[GC_ME_MYSTERIOUS_CHEST\]/s/10,180,1,2,/10,180,1,3,/"
plant salesman-money "/\[GC_ME_THE_POKEMON_SALESMAN\]/s/4.0/2.0/"
plant teleport-modulus "/\[GC_ME_TELEPORTING_HIJINKS\]/s/(1u<<2)|(1u<<3)|(1u<<4)/(1u<<1)|(1u<<2)|(1u<<3)/"
plant dark-deal-challenge "/\[GC_ME_DARK_DEAL\]/s/(1u<<0)|(1u<<9)/(1u<<0)/"
plant breeder-hardcore "/\[GC_ME_THE_EXPERT_POKEMON_BREEDER\]/s/(1u<<9)/(1u<<8)/"
plant snorlax-challenge "/\[GC_ME_SLUMBERING_SNORLAX\]/s/(1u<<11)/0/"
echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
