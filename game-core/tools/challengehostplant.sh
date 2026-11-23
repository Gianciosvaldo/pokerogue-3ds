#!/bin/sh
# tests/challenge_host_test.c: show it can fail. Each plant changes one line of a copy of game-core; every copy's test must exit 1.
# Usage: tools/challengehostplant.sh
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-challenge-host-plant}
fails=0; total=0
plant() {
  NAME=$1; FILE=$2; EXPR=$3
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/challenge_host_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/challenge_host_test" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st"
  total=$((total + 1)); [ "$st" = 1 ] || fails=$((fails + 1))
}
plant played-mask-inverse gc_challenge.c 's/return 1u << CHALLENGES_FRESH_START;/return (1u << CHALLENGES_FRESH_START) | (1u << CHALLENGES_INVERSE_BATTLE);/'
plant nuzlocke-without-hardcore gc_challenge.c 's/v\[CHALLENGES_LIMITED_CATCH\] \&\& v\[CHALLENGES_HARDCORE\]) f |= GC_RIBBON_NUZLOCKE/v[CHALLENGES_LIMITED_CATCH]) f |= GC_RIBBON_NUZLOCKE/'
plant nuzlocke-without-fresh gc_challenge.c 's/v\[CHALLENGES_FRESH_START\] \&\& v\[CHALLENGES_LIMITED_CATCH\] \&\& v\[CHALLENGES_HARDCORE\]) f |= GC_RIBBON_NUZLOCKE/v[CHALLENGES_LIMITED_CATCH] \&\& v[CHALLENGES_HARDCORE]) f |= GC_RIBBON_NUZLOCKE/'
plant flip-inverse-keeps-others gc_challenge.c 's/if (flip_or_inverse) return flip_or_inverse;/if (flip_or_inverse) return f;/'
plant fresh-max-3 gc_challenge.c 's/    2,  \/\* FRESH_START \*\//    3,  \/* FRESH_START *\//'
plant moveset-randomizer-ribbons gc_challenge.c 's/if (v\[CHALLENGES_MOVESET_RANDOMIZER\]) return 0;//'
plant save-no-value-check gc_save.c 's/||!gc_challenge_set_valid(&chal,NULL)//'
plant save-takes-unplayed gc_save.c 's/if (chal_flags \& ~gc_challenge_played_mask())/if (0)/'
plant finish-takes-unplayed gc_classic.c 's/gc_challenges_unplayed(run)/0/'
plant party-never-valid gc_challenge.c 's/        any_valid |= valid;/        any_valid |= 0;/'
# (the "no member is hard-valid" branch of gc_challenge_party_ok is not reachable with Fresh Start alone: every member is hard-valid or refused first;
#  it gets its test and plant with Single Type, the first challenge with a soft-valid member)
echo "challenge host plants: $total run, $fails not caught"
[ "$fails" = 0 ]
