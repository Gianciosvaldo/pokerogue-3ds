#!/bin/sh
# Trainer Terastallization: show the checks can fail. Replays one trace (from tools/trainerteradiff.sh) through copies of game-core, each with one line
# of src/gc_battle.c changed, and runs the differential (tests/trainer_tera_diff_test.c) and the host test (tests/trainer_tera_test.c) on it.
# Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Usage: tools/trainerteraplant.sh <trace>
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-trainer-tera-plant}
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_battle.c}
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/trainer_tera_diff_test" "$W/build/trainer_tera_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  d=0; h=0
  "$W/build/trainer_tera_diff_test" "$TRACE" >"$W/diff.txt" 2>&1 || d=$?
  "$W/build/trainer_tera_test" >"$W/host.txt" 2>&1 || h=$?
  who=none; [ "$d" = 1 ] && who=diff; [ "$h" = 1 ] && { [ "$who" = none ] && who=host || who=diff+host; }
  echo "$NAME: caught by $who (diff exit $d: $(grep 'TOTAL' "$W/diff.txt" | cut -c1-60); host exit $h: $(tail -1 "$W/host.txt" | cut -c1-80))"
  total=$((total + 1))
  [ "$who" != none ] || fails=$((fails + 1))
}
plant shedinja-always-ok     's/const bool shedinja_ok = tc->tera_type < 0 || tc->tera_type == POKEMON_TYPE_BUG;/const bool shedinja_ok = true;/'
plant shedinja-never-ok      's/const bool shedinja_ok = tc->tera_type < 0 || tc->tera_type == POKEMON_TYPE_BUG;/const bool shedinja_ok = false;/'
plant gym-wave-100-exclusive 's/run->wave < tc->tera_min_wave/run->wave <= tc->tera_min_wave/'
plant tera-slot-ignored      's/const int slot = tc->tera_slot\[n\];/const int slot = -1;/'
plant type-not-given         's/if (tc->tera_type >= 0) run->enemy\[pick\].b.tera_type = tc->tera_type;/if (0) run->enemy[pick].b.tera_type = tc->tera_type;/'
plant static-slots-dropped   's/if (tc->tera_static \& (1u << i)) out\[n++\] = (int8_t)i;/if (0) out[n++] = (int8_t)i;/'
plant pick-flag-dropped      's/if (run->enemy\[i\].tera_instant) out\[n++\] = (int8_t)i;/if (0) out[n++] = (int8_t)i;/'
plant pick-never-first       's/pr_rng_integer_in_range(&run->run.rng, 0, ni - 1)/pr_rng_integer_in_range(\&run->run.rng, 1, ni - 1)/'
plant single-candidate-draws 's/idx\[ni == 1 ? 0 : pr_rng_integer_in_range/idx[0 ? 0 : pr_rng_integer_in_range/'
plant picks-last-candidate   's/pr_rng_integer_in_range(&run->run.rng, 0, ni - 1)\];/ni - 1];/'
plant mode-never-instant     's/in->ai_trainer.tera_mode = in->ai_trainer.n_instant_teras ? PR_TERA_AI_INSTANT_TERA : PR_TERA_AI_NO_TERA;/in->ai_trainer.tera_mode = PR_TERA_AI_NO_TERA;/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "planted bugs caught: $total of $total"
