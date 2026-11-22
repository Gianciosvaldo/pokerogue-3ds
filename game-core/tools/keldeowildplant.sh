#!/bin/sh
# A wild Keldeo's form change: show the comparator can fail. Replays traces (from tools/keldeowilddiff.sh) through copies of game-core, each with one line of
# src/gc_moveset.c changed, and prints each one's totals. Every copy must exit 1 (a plant counts as caught when any trace fails).
# Not planted, because no trace reaches them: the form-already-set no-op (the caller skips it), the hidden-ability slot count (Keldeo has none), an ability override,
# the Disguise text (Keldeo has no Disguise), the apply loop's same-form skip (the quiet change returns anyway), the dex seen count (the comparator reads the flags), a form with half-form state, other species.
# Usage: GC_LOCALES=<upstream>/locales/en tools/keldeowildplant.sh <trace> [<trace>...]
set -e
: "${1:?trace}"; : "${GC_LOCALES:?GC_LOCALES}"
TRACES=""
for t in "$@"; do TRACES="$TRACES $(CDPATH= cd -- "$(dirname "$t")" && pwd)/$(basename "$t")"; done
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-keldeowild-plant}
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_moveset.c}
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/keldeo_wild_diff_test" >/dev/null 2>&1
  st=0; tot=""
  for TRACE in $TRACES; do
    s1=0; "$W/build/keldeo_wild_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || s1=$?
    tot="$tot $(grep '^TOTAL' "$W/out.txt" | tr -d '\n')"
    [ "$s1" = 1 ] && st=1
  done
  echo "$NAME: exit $st,$tot"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
}
plant mode-ignored 's/run->bt.game_mode = run->run.mode;/run->bt.game_mode = 0;/'
plant ability-clamp 's/m->ability_index = (uint8_t)(count - 1);/m->ability_index = (uint8_t)(count - 2);/'
plant seen-skipped 's/if (run->sys) gc_system_set_pokemon_seen/if (0 \&\& run->sys) gc_system_set_pokemon_seen/'
plant revert-text 's/if (form == 0) gc_ev_msg(run, "battlePokemonForm:revertChange"/if (form == 1) gc_ev_msg(run, "battlePokemonForm:revertChange"/'
plant pre-name 's/gc_m_mon(run, "preName", 1, idx);/gc_m_mon(run, "pokemonName", 1, idx);/'
plant queued-ignored 's/return queued ? target : -1;/return target;/'
echo "planted: $total, not caught: $fails"
[ "$fails" = 0 ]
