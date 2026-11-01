#!/bin/sh
# The evil teams' fixed fights in the run: show the checks can fail. Replays one trace (from tools/evildiff.sh) through copies of game-core and run-core,
# each with one line of src/newbattle.c (run-core) or src/gc_encounter.c (game-core) changed, and runs both the differential (tests/gym_e4_diff_test.c)
# and the host test (tests/gym_e4_test.c) on it. Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Not planted (equivalent under both): the order of the ctx assignments, the comments, the party template roll in trainer_ctor (the Youngster and the grunts have
# one template, so randSeedItem rolls nothing).
# Usage: tools/evilplant.sh <trace>   (EVIL_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-evil-plant}
fails=0; total=0
started=0
# plant NAME SED-EXPRESSION [FILE under game-core/src, or run-core/src/<file> as "run-core:<file>"]
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_encounter.c}
  [ "$NAME" != "${EVIL_PLANT_FROM:-}" ] || started=1
  [ -z "${EVIL_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core" "$W/run-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  (cd "$ROOT/run-core" && tar cf - --exclude=./build .) | tar xf - -C "$W/run-core"
  for d in battle-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  # the libraries already built (make rebuilds what is out of date: the changed copy of run-core always is)
  mkdir -p "$W/build"
  for d in ai-core reward-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  case "$FILE" in
    run-core:*) ORIG="$ROOT/run-core/src/${FILE#run-core:}"; COPY="$W/run-core/src/${FILE#run-core:}" ;;
    *) ORIG="$HERE/src/$FILE"; COPY="$W/game-core/src/$FILE" ;;
  esac
  sed -i "$EXPR" "$COPY"
  n=$(diff "$ORIG" "$COPY" | grep -c '^>' || true)
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
N=run-core:newbattle.c
plant grunt-pool-short      's/evil_grunts\[seed_item(&r, 10)\]/evil_grunts[seed_item(\&r, 9)]/' $N
plant grunt-offset-per-wave 's/(fixed_evil == 1 ? wave : CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1) << 8/wave << 8/' $N
plant youngster-offset      's/(fixed_evil == 1 ? wave :/(fixed_evil == 1 ? wave + 1 :/' $N
plant youngster-gender      's/out->variant = pr_rng_seed_int(&r, 2, 0) ?/out->variant = !pr_rng_seed_int(\&r, 2, 0) ?/' $N
plant gender-roll-inverted  's/ctx->evil_gender_roll == 0 ? TRAINER_VARIANT_FEMALE/ctx->evil_gender_roll != 0 ? TRAINER_VARIANT_FEMALE/' $N
plant double-roll-shifted   's/ctx->evil_double_roll == 0) out->variant/ctx->evil_double_roll == 1) out->variant/' $N
plant double-never          's/if (t->has_double \&\& ctx->evil_double_roll == 0)/if (0 \&\& t->has_double \&\& ctx->evil_double_roll == 0)/' $N
plant names-wrong-gender    's/(out->variant == TRAINER_VARIANT_FEMALE ? t->names_female : t->names_male)/(out->variant == TRAINER_VARIANT_FEMALE ? t->names_male : t->names_female)/' $N
plant grunts-not-fixed      's/const int fixed_evil = ctx->enable_evil_grunts ? evil_grunt_kind(run->mode, wave) : 0;/const int fixed_evil = 0;/' $N
plant evil-off              's/    ctx.enable_evil_grunts = true;/    ctx.enable_evil_grunts = false;/'
plant pin-ignored           's/in->evil_roll_pinned ? in->evil_roll\[0\] : //'
plant last-grunt-no-roll    's/wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_4) {/wave == 0) {/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
