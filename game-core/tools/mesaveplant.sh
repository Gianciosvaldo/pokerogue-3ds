#!/bin/sh
# An encounter's one save, at its first question (gc_run_can_save / gc_run_save / gc_run_load, src/gc_save.c and gc_phases.c): show the tests can fail. Each copy of game-core has
# one line changed; it is built with the host test (tests/mystery_save_test.c) and the soak (gcplay --save-test --force-mystery), and every copy must fail one of them:
# the host test exits non-zero, or `make me-save-soak` does (a run's outcome changed with the save and load at the question, or too few questions were saved).
# Usage: tools/mesaveplant.sh      (MESAVE_PLANT_ONLY="name name" runs only those)
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-mesave-plant}
fails=0; total=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_save.c}
  if [ -n "$MESAVE_PLANT_ONLY" ]; then case " $MESAVE_PLANT_ONLY " in *" $NAME "*) ;; *) return 0 ;; esac; fi
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  m=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^<' || true)
  [ "$n" -le 1 ] && [ "$m" = 1 ] || { echo "$NAME: the change didn't apply to one line ($m removed, $n added)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_save_test" "$W/build/gcplay" >/dev/null 2>&1 || { echo "$NAME: does not build"; exit 2; }
  st=0; "$W/build/mystery_save_test" >"$W/out.txt" 2>&1 || st=$?
  how="host test: $(tail -1 "$W/out.txt" | cut -c1-110)"
  if [ "$st" = 0 ]; then
    st=0; make -s -C "$W/game-core" BUILD="$W/build" me-save-soak >"$W/soak.txt" 2>&1 || st=$?
    how="soak: $(grep -E 'changed with|stuck|encounter-question saves' "$W/soak.txt" | head -2 | tr '\n' ' ' | cut -c1-150)"
  fi
  echo "$NAME: exit $st, $how"
  total=$((total + 1))
  [ "$st" != 0 ] || fails=$((fails + 1))
}
# the question is never marked, so nothing can be saved at it
plant question-never-marked 's/^        in->me_question = true;$/        in->me_question = false;/' gc_phases.c
# can_save asks for the wrong screen / the wrong phase / a stale run
plant can-save-wrong-screen 's/|| run->screen.kind != GC_SCREEN_MYSTERY || run->mystery_type < 0/|| run->screen.kind != GC_SCREEN_COMMAND || run->mystery_type < 0/'
plant can-save-wrong-phase 's/|| mq->current.type != RP_MYSTERY || run->n_enemy < 0/|| mq->current.type != RP_COMMAND || run->n_enemy < 0/'
plant can-save-not-at-save-point 's/if (!in->at_save_point || !run->in_battle || run->screen.kind != GC_SCREEN_MYSTERY/if (in->at_save_point || !run->in_battle || run->screen.kind != GC_SCREEN_MYSTERY/'
plant can-save-wrong-battle-type 's/|| run->battle_type != BATTLE_TYPE_MYSTERY_ENCOUNTER || mq->current.type/|| run->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER || mq->current.type/'
# an answer does not end the save point
plant answer-keeps-question 's/^            if (\*ok) gc_int(run)->me_question = false;.*$/            (void)0;/' gc_phases.c
# the marker
plant marker-wrong-value 's/^        const uint8_t me_question = 1;$/        const uint8_t me_question = 0;/'
plant marker-never-written 's/^    if (in->me_question) {   \/\* optional marker.*$/    if (0) {/'
# the load
plant load-no-mystery-event 's/^        me_ev = gc_ev_new(run, GC_EV_MYSTERY);$/        me_ev = gc_ev_new(run, GC_EV_SAVE_POINT);/'
plant load-no-wave-event 's/^        gc_event \*me_ev = gc_ev_new(run, GC_EV_WAVE);$/        gc_event *me_ev = gc_ev_new(run, GC_EV_SAVE_POINT);/'
plant load-no-save-point 's/^        gc_ev_new(run, GC_EV_SAVE_POINT);$/        (void)0;/'
plant load-not-at-save-point 's/^        in->at_save_point = true;$/        in->at_save_point = false;/'
plant load-queue-swapped 's/gc_rq_unshift(run, gc_rp(RP_MYSTERY, run->mystery_type, run->mystery_tier, 0));/gc_rq_unshift(run, gc_rp(RP_MYSTERY, run->mystery_tier, run->mystery_type, 0));/'
plant load-no-party-list 's/^        for (int k = 0; k < run->n_party; k++) bt->party\[0\]\[k\] = .*$/        (void)0;/'
plant load-no-party-count 's/^        bt->n_party\[0\] = (uint8_t)run->n_party;$/        (void)0;/'
plant load-no-items-to-battle 's/^        gc_items_to_battle(run);$/        (void)0;/'
plant load-no-battle-rng 's/^        bt->battle_rng = v.battle_rng;   \/\*.*$/        (void)0;/'
plant load-no-battle-rng-live 's/^        bt->battle_rng_live = v.battle_rng_live;$/        (void)0;/'
plant load-no-bt-rng 's/^            bt->rng = btrng.rng;$/            (void)0;/'
plant load-no-bt-rng-live 's/^            bt->rng_live = btrng.live != 0;$/            (void)0;/'
plant save-no-bt-rng 's/^        sv_me_btrng btrng = {bt->rng, bt->rng_live};$/        sv_me_btrng btrng = {{0}, 0};/'
plant load-no-stats 's/^        if (!r.bad) get_block(&r, stat_fields, NF(stat_fields), run, sizeof \*run, 1, &dropped);$/        (void)0;/'
plant load-no-ai-reset 's/^        gc_ai_reset(run);   \/\* the AI.*$/        (void)0;/'
plant load-no-weird-reset 's/^        gc_weird_reset_ranges();   \/\* Weird Dream.*$/        (void)0;/'
plant load-drops-question-check 's/    if (me_q \&\& (!v.in_battle || terminal.terminal_result || v.battle_type != BATTLE_TYPE_MYSTERY_ENCOUNTER || v.mystery_type < 0 || v.mystery_type >= GC_ME_COUNT/    if (0 \&\& (!v.in_battle || terminal.terminal_result || v.battle_type != BATTLE_TYPE_MYSTERY_ENCOUNTER || v.mystery_type < 0 || v.mystery_type >= GC_ME_COUNT/'
# a load counts the question's setup a second time in the not-ported counters (the saved ones already hold the first)
plant load-counts-setup-twice 's/^        in->me_loaded = true;$/        in->me_loaded = false;/'
# a Challenge Mode question is refused by the load (upstream: hasMysteryEncounters is CLASSIC and CHALLENGE)
plant load-refuses-challenge-question 's/|| v.me_battle || !gc_is_classic_mode(v.mode))) {/|| v.me_battle || v.mode != GAME_MODES_CLASSIC)) {/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
