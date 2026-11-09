#!/bin/sh
# The Bug-Type Superfan's move tutor after the win: show the host test can fail. Builds copies of game-core, each with one line changed, and runs tests/mystery_superfan_test.c on each (the picker is a screen, so there is no upstream trace to compare: the four moves it offers are compared by tools/superfanfightdiff.sh). Every copy must make the host test fail.
# Not planted (equivalent mutants): clearing the screen in the choice (gc_run_choose runs on to the next screen right after, which replaces it).
# Usage: tools/superfantutorplant.sh   (SUPERFAN_PLANT_FROM=<name> starts at that plant, SUPERFAN_PLANT_ONLY="a b" runs those)
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-superfan-tutor-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_superfan.c}
  [ "$NAME" != "${SUPERFAN_PLANT_FROM:-}" ] || started=1
  [ -z "${SUPERFAN_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${SUPERFAN_PLANT_ONLY:-}" ] || case " $SUPERFAN_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  case "$FILE" in
    battle-core/*) rm "$W/battle-core"; (cd "$ROOT" && tar cf - --exclude=./battle-core/build battle-core) | tar xf - -C "$W"; TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *) TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_superfan_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_superfan_test" >"$W/host.txt" 2>&1 || st=$?
  by=host-test
  [ "$st" = 0 ] || st=1
  echo "$NAME: exit $st ($by)"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

plant pending-set 's#in->superfan.tutor_pending = true;#in->superfan.tutor_pending = false;#'
plant pending-cleared 's#^    in->superfan.tutor_pending = false;#    ;#'
plant before-flag 's#in->me_reward.before = true;#in->me_reward.before = false;#'
plant start-guard 's#if (!in->superfan.tutor_pending) return false;#if (in->superfan.tutor_pending) return false;#'
plant start-message 's#bugTypeSuperfan:battleWon#bugTypeSuperfan:outro#'
plant start-options 's#s->n_options = 5;#s->n_options = 4;#'
plant start-cancel 's#for (int k = 0; k < 5; k++) s->option_ok\[k\] = true;#for (int k = 0; k < 4; k++) s->option_ok[k] = true;#'
plant start-pick 's#s->option_pick\[k\] = GC_PICK_MEMBER;#s->option_pick[k] = GC_PICK_NONE;#'
plant start-stage 's#s->me_stage = GC_ME_STAGE_TUTOR;#s->me_stage = GC_ME_STAGE_FIRST;#'
plant start-kind 's#s->kind = GC_SCREEN_MYSTERY;#s->kind = GC_SCREEN_REWARD;#'
plant choose-kind 's#if (c->kind != GC_CH_MYSTERY) { \*why = "choose a move to teach"; return false; }#;#'
plant choose-range 's#if (c->a < 0 || c->a > 4)#if (c->a < 0 || c->a > 5)#'
plant choose-low 's#if (c->a < 0 || c->a > 4)#if (c->a < -1 || c->a > 4)#'
plant choose-member 's#(c->b < 0 || c->b >= run->n_party)) { \*why = "no such Pokemon"#(c->b < 0 || c->b > run->n_party)) { *why = "no such Pokemon"#'
plant choose-member-low 's#(c->b < 0 || c->b >= run->n_party)) { \*why = "no such Pokemon"#(c->b < -1 || c->b >= run->n_party)) { *why = "no such Pokemon"#'
plant choose-cancel 's#if (c->a < 4) gc_rq_unshift#if (c->a < 3) gc_rq_unshift#'
plant choose-learner 's#gc_rp(RP_LEARN_MOVE, c->b, gc_int(run)->superfan.tutor\[c->a\], 0)#gc_rp(RP_LEARN_MOVE, 0, gc_int(run)->superfan.tutor[c->a], 0)#'
plant choose-move 's#gc_rp(RP_LEARN_MOVE, c->b, gc_int(run)->superfan.tutor\[c->a\], 0)#gc_rp(RP_LEARN_MOVE, c->b, gc_int(run)->superfan.tutor[0], 0)#'
plant accessor 's#return k >= 0 \&\& k < 4 ? gc_int((gc_run \*)run)->superfan.tutor\[k\] : 0;#return k >= 0 \&\& k < 4 ? gc_int((gc_run *)run)->superfan.tutor[3 - k] : 0;#'
plant pick-option 's#return option < 4 \&\& idx >= 0 \&\& idx < run->n_party;#return idx >= 0 \&\& idx < run->n_party;#' gc_mystery.c
plant pick-range 's#return option < 4 \&\& idx >= 0 \&\& idx < run->n_party;#return option < 4 \&\& idx >= 0 \&\& idx <= run->n_party;#' gc_mystery.c
plant pick-stage 's#if (run->screen.me_stage == GC_ME_STAGE_TUTOR \&\& type == GC_ME_BUG_TYPE_SUPERFAN)#if (0 \&\& type == GC_ME_BUG_TYPE_SUPERFAN)#' gc_mystery.c
plant hook-start 's#if (gc_superfan_tutor_start(run)) return false;#;#' gc_phases.c
plant hook-start-wait 's#if (gc_superfan_tutor_start(run)) return false;#gc_superfan_tutor_start(run);#' gc_phases.c
plant hook-choose 's#return gc_superfan_tutor_choose(run, c, why, ok);#*why = "nothing to answer"; return false;#' gc_phases.c

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
