#!/bin/sh
# Gym leaders / Elite Four / Champion in the run: show the checks can fail. Replays one trace (from tools/gymdiff.sh) through copies of game-core, each
# with one line of src/gc_encounter.c, src/gc_battle.c or src/gc_phases.c changed, and runs both the differential (tests/gym_e4_diff_test.c) and the host test
# (tests/gym_e4_test.c) on it. Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Not planted (equivalent under both): the order of the ctx assignments, the comments.
# Usage: tools/gymplant.sh <trace>   (GYM_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-gym-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_encounter.c}
  [ "$NAME" != "${GYM_PLANT_FROM:-}" ] || started=1
  [ -z "${GYM_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  # the other cores are not changed: start from the libraries already built (make rebuilds what is out of date)
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/gym_e4_diff_test" "$W/build/gym_e4_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  d=0; h=0
  "$W/build/gym_e4_diff_test" "$TRACE" >"$W/diff.txt" 2>&1 || d=$?
  "$W/build/gym_e4_test" >"$W/host.txt" 2>&1 || h=$?
  who=none; [ "$d" = 1 ] && who=diff; [ "$h" = 1 ] && { [ "$who" = none ] && who=host || who=diff+host; }
  echo "$NAME: caught by $who (diff exit $d: $(grep 'TOTAL' "$W/diff.txt"); host exit $h: $(tail -1 "$W/host.txt" | cut -c1-90))"
  total=$((total + 1))
  [ "$who" != none ] || fails=$((fails + 1))
}
plant elite-four-off       's/    ctx.enable_gym_e4 = true;/    ctx.enable_gym_e4 = false;/'
plant gym-parties-off      's/    tc.enable_gym_e4 = true;/    tc.enable_gym_e4 = false;/'
plant biome-ignored        's/    ctx.biome = run->biome;/    ctx.biome = BIOME_ID_PLAINS;/'
plant wave20-block-moved   's/tc.classic_wave20_trainer = run->wave == 20 \&\&/tc.classic_wave20_trainer = run->wave == 21 \&\&/'
plant levels-dropped       's/    memcpy(tc.levels, levels, sizeof(int) \* (size_t)n_levels);/    for (int q = 0; q < n_levels; q++) tc.levels[q] = levels[q] - 1;/'
plant ai-never-boss        's/    in->ai_trainer.is_boss = tc \&\& tc->is_boss;/    in->ai_trainer.is_boss = false;/' gc_battle.c
plant ai-always-boss       's/    in->ai_trainer.is_boss = tc \&\& tc->is_boss;/    in->ai_trainer.is_boss = tc != NULL;/' gc_battle.c
plant speaker-left-to-the-run 's/    if (speaker) {/    if (speaker \&\& 0) {/'
plant speaker-wrong-name   's/    const char \*speaker = run->trainer_names\[kind == 0 ? GC_TNAME_TITLED : GC_TNAME_SPEAKER\];/    const char *speaker = run->trainer_names[GC_TNAME_TITLED];/'
plant defeated-type-unset  's/    if (defeated) defeated->a = run->trainer_type;.*/    if (defeated) defeated->a = -1;/' gc_phases.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
