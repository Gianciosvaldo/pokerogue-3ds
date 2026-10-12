/* observer_test: the combat observer (source/combat_observer.c) on literal event fixtures, on the host.
 *
 *   make observer_test && build/observer_test
 *
 * Feeds the observer the same things run_screen.c's drain() does (game-core's WAVE / TURN / BATTLE events and battle-core's pr_event
 * list) and reads back the history custom-core's reducer built from them. The fixtures: a battle in several drains with a switch,
 * a faint after poison and one after a hit with a move animation before it (no source is ever named), a heal, an HP event of 0, a
 * 70-event turn and six turns, the same battle number in two waves, a replayed and a skipped drain, a game that says it lost
 * events, a range outside the buffer, events the reducer cannot take, a run that ends twice, a new run after one, and a resumed run.
 * Every history is also checked with custom-core's own validators. Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "combat_observer.h"
#include "cc/model.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ builders */

enum { PIKACHU = 25, PIDGEY = 16, RATTATA = 19, CATERPIE = 10 };

static pr_event ev(int type, int battler, int species, int hp, int max_hp) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = (uint8_t)type;
  e.battler = (int8_t)battler;
  e.target = -1;
  if (battler >= 0 && battler < 4) {
    e.field[battler].species = (uint16_t)species;
    e.field[battler].hp = hp;
    e.field[battler].max_hp = max_hp;
  }
  return e;
}
static pr_event damage(int battler, int species, int hp, int max_hp, int amount) {
  pr_event e = ev(PR_EV_DAMAGE, battler, species, hp, max_hp);
  e.id = 1; /* HIT_RESULT_EFFECTIVE */
  e.value = amount;
  return e;
}
static pr_event hp_event(int battler, int species, int hp, int max_hp, int value) {
  pr_event e = ev(PR_EV_HP, battler, species, hp, max_hp);
  e.value = value;
  return e;
}
static pr_event faint(int battler, int species, int max_hp) { return ev(PR_EV_FAINT, battler, species, 0, max_hp); }
static pr_event switch_in(int battler, int species, int hp, int max_hp) {
  pr_event e = ev(PR_EV_SWITCH_IN, battler, species, hp, max_hp);
  e.value = battler;
  return e;
}
static pr_event switch_out(int battler, int species, int hp, int max_hp) { return ev(PR_EV_SWITCH_OUT, battler, species, hp, max_hp); }
static pr_event move_anim(int user, int target, int species_user, int move) {
  pr_event e = ev(PR_EV_MOVE_ANIM, user, species_user, 30, 30);
  e.target = (int8_t)target;
  e.id = (uint16_t)move;
  return e;
}
static pr_event text(void) { return ev(PR_EV_TEXT, -1, 0, 0, 0); }

static gc_event gev(int kind, int a, int b) {
  gc_event g;
  memset(&g, 0, sizeof g);
  g.kind = (uint8_t)kind;
  g.a = a;
  g.b = b;
  return g;
}

/* One drain: the gc events, and the battle events they point at */
static uint32_t drain_no;
static void drain(const gc_event* q, int nq, const pr_event* be, int n_be) { co_observe(++drain_no, q, nq, be, n_be, false, false); }

static void begin(bool resumed) {
  co_begin_run(resumed);
  drain_no = 0;
}

static const co_report* finish(bool victory) {
  CHECK(co_finish(victory, 1, 7), "finish");
  const co_report* r = co_report_get();
  CHECK(r != NULL, "no report");
  if (r) {
    CHECK(cc_history_validate(&r->history) == CC_OK, "history invalid: %s", cc_status_str(cc_history_validate(&r->history)));
    for (int t = 0; t < r->history.n_turns; t++) CHECK(cc_turn_validate(&r->history.turns[t]) == CC_OK, "turn %d invalid", t);
  }
  return r;
}

/* ------------------------------------------------------------------ fixtures */

/* A wild battle over three drains: the intro, a turn that ends in a switch prompt, the rest of that turn after the answer. */
static void test_battle_in_drains(void) {
  begin(false);
  /* drain 1: the wave and the intro (no TURN yet: turn 0). Pikachu 55 HP out, a wild Pidgey. A text and a move animation come with
   * them and are not stored. */
  pr_event be1[] = {text(), switch_in(0, PIKACHU, 55, 55), switch_in(2, PIDGEY, 40, 40), text()};
  gc_event q1[] = {gev(GC_EV_WAVE, 3, 1), gev(GC_EV_BATTLE, 0, 4)};
  drain(q1, 2, be1, 4);
  /* drain 2: turn 1. Pikachu is hit for 24 (55 -> 31), hits back for 18 (40 -> 22), then heals 10 (31 -> 41) */
  pr_event be2[] = {move_anim(2, 0, PIDGEY, 98), damage(0, PIKACHU, 31, 55, 24), move_anim(0, 2, PIKACHU, 85), damage(2, PIDGEY, 22, 40, 18),
                    hp_event(0, PIKACHU, 41, 55, 10)};
  gc_event q2[] = {gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 5)};
  drain(q2, 2, be2, 5);
  /* drain 3: turn 2, poison takes 8 (41 -> 33, no hit event), Pikachu faints later in the same turn after a hit */
  pr_event be3[] = {hp_event(0, PIKACHU, 33, 55, -8), damage(0, PIKACHU, 0, 55, 33), faint(0, PIKACHU, 55)};
  gc_event q3[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, 0, 3)};
  drain(q3, 2, be3, 3);
  /* drain 4: the rest of turn 2 after the player's switch prompt: no TURN event, it is still turn 2 */
  pr_event be4[] = {switch_in(0, RATTATA, 30, 30)};
  gc_event q4[] = {gev(GC_EV_BATTLE, 0, 1)};
  drain(q4, 1, be4, 1);

  const co_stats* st = co_stats_get();
  CHECK(st->drains == 4 && st->ranges == 4 && st->batches == 4 && st->accepted == 4, "counts %u/%u/%u/%u", st->drains, st->ranges, st->batches, st->accepted);
  CHECK(st->rejected == 0 && st->unrepresentable == 0 && st->bad_range == 0 && st->unkeyed == 0 && st->loss_flagged == 0, "nothing lost");
  const co_report* r = finish(false);
  if (!r) return;
  const cc_history* h = &r->history;
  CHECK(h->n_turns == 3, "turns %d", h->n_turns);
  if (h->n_turns != 3) return;
  /* turn 0 (the intro): two switch-ins */
  CHECK(h->turns[0].turn == 0 && h->turns[0].wave == 3 && h->turns[0].battle_seq == 1 && h->turns[0].n_events == 2 && h->turns[0].flags == 0, "turn 0");
  CHECK(h->turns[0].events[0].kind == CC_EV_SWITCH_IN && h->turns[0].events[0].target_side == CC_SIDE_PLAYER && h->turns[0].events[0].target_species == PIKACHU, "intro 0");
  CHECK(h->turns[0].events[1].kind == CC_EV_SWITCH_IN && h->turns[0].events[1].target_side == CC_SIDE_ENEMY && h->turns[0].events[1].target_species == PIDGEY, "intro 1");
  /* turn 1: hit, hit, heal. The move animations are not events and name nobody */
  const cc_turn* t1 = &h->turns[1];
  CHECK(t1->turn == 1 && t1->n_events == 3 && t1->flags == 0, "turn 1 has %d events", t1->n_events);
  CHECK(t1->events[0].kind == CC_EV_DAMAGE && t1->events[0].target_side == CC_SIDE_PLAYER && (t1->events[0].known & CC_EK_SHOWN) && t1->events[0].shown == 24, "hit 1");
  CHECK(t1->events[1].kind == CC_EV_DAMAGE && t1->events[1].target_side == CC_SIDE_ENEMY && t1->events[1].shown == 18, "hit 2");
  CHECK(t1->events[2].kind == CC_EV_HEAL && t1->events[2].shown == 10, "heal shown");
  for (int k = 0; k < t1->n_events; k++)
    CHECK(!(t1->events[k].known & (CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE)), "event %d names a source", k);
  /* the measured movement of the first hit is the snapshot difference (55 -> 31) and of the second (40 -> 22); the displayed number is a separate field */
  CHECK((t1->events[0].known & CC_EK_HP_DELTA) && t1->events[0].hp_delta == -24, "measured 1");
  CHECK((t1->events[1].known & CC_EK_HP_DELTA) && t1->events[1].hp_delta == -18, "measured 2");
  CHECK((t1->events[2].known & CC_EK_HP_DELTA) && t1->events[2].hp_delta == 10, "measured heal");
  /* turn 2: HP loss without a hit (indirect: no source, the engine's -8), a hit, a faint, then the switch-in after the prompt */
  const cc_turn* t2 = &h->turns[2];
  CHECK(t2->turn == 2 && t2->n_events == 4 && t2->flags == 0, "turn 2 has %d events", t2->n_events);
  CHECK(t2->events[0].kind == CC_EV_HP_LOSS && t2->events[0].hp_delta == -8 && !(t2->events[0].known & CC_EK_SHOWN), "poison");
  CHECK(!(t2->events[0].known & (CC_EK_SOURCE_SIDE | CC_EK_MOVE)), "poison has a cause");
  CHECK(t2->events[1].kind == CC_EV_DAMAGE && t2->events[1].shown == 33, "last hit");
  CHECK(t2->events[2].kind == CC_EV_FAINT && !(t2->events[2].known & (CC_EK_SOURCE_SIDE | CC_EK_SOURCE_ID | CC_EK_SOURCE_SPECIES | CC_EK_MOVE)), "faint has a source");
  CHECK(t2->events[3].kind == CC_EV_SWITCH_IN && t2->events[3].target_species == RATTATA, "switch-in after the prompt");
  CHECK(r->fallen_known && r->fallen_species == PIKACHU, "last fallen");
  CHECK(r->outcome == CC_OUT_DEFEAT && r->wave == 7 && r->biome == 1 && !r->began_mid && !r->untracked_loss, "report header");
  CHECK(st->events_in == 9, "events_in %u (2 + 3 + 3 + 1; texts and move animations are not given to the reducer)", st->events_in);
}

/* Switching: a species replaced in a position is a new tracker entry (no measured delta against the old one), and the leaver is stored */
static void test_switch_and_reused_position(void) {
  begin(false);
  pr_event be[] = {switch_in(0, PIKACHU, 50, 50),  switch_in(2, PIDGEY, 40, 40), damage(0, PIKACHU, 20, 50, 30), switch_out(0, PIKACHU, 20, 50),
                   switch_in(0, RATTATA, 30, 30),  damage(0, RATTATA, 25, 30, 5)};
  gc_event q[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 6)};
  drain(q, 3, be, 6);
  const co_report* r = finish(false);
  if (!r || r->history.n_turns != 1) { CHECK(0, "one turn expected"); return; }
  const cc_turn* t = &r->history.turns[0];
  CHECK(t->n_events == 6, "events %d", t->n_events);
  CHECK(t->events[2].kind == CC_EV_DAMAGE && (t->events[2].known & CC_EK_HP_DELTA) && t->events[2].hp_delta == -30, "hit measured against the switch-in");
  CHECK(t->events[3].kind == CC_EV_SWITCH_OUT && t->events[3].target_species == PIKACHU, "leaver stored with its own species");
  CHECK(t->events[4].kind == CC_EV_SWITCH_IN && t->events[4].target_species == RATTATA, "entrant");
  /* Rattata came in at 30/30; the next hit is measured against that, not against Pikachu's 20 */
  CHECK(t->events[5].kind == CC_EV_DAMAGE && (t->events[5].known & CC_EK_HP_DELTA) && t->events[5].hp_delta == -5, "hit measured against Rattata, not Pikachu: %d", (int)t->events[5].hp_delta);
}

/* A hit, a move animation and a faint: the animation proves nothing, and nothing gets a source. */
static void test_unknown_source(void) {
  begin(false);
  pr_event be[] = {switch_in(0, PIKACHU, 12, 50), switch_in(2, PIDGEY, 40, 40), move_anim(2, 0, PIDGEY, 98), move_anim(2, 0, PIDGEY, 98), faint(0, PIKACHU, 50)};
  gc_event q[] = {gev(GC_EV_WAVE, 9, 0), gev(GC_EV_TURN, 4, 0), gev(GC_EV_BATTLE, 0, 5)};
  drain(q, 3, be, 5);
  const co_report* r = finish(false);
  if (!r || r->history.n_turns != 1) { CHECK(0, "one turn expected"); return; }
  const cc_turn* t = &r->history.turns[0];
  CHECK(t->n_events == 3, "two switch-ins and a faint, no animation: %d", t->n_events);
  CHECK(t->events[2].kind == CC_EV_FAINT && !(t->events[2].known & CC_EK_SOURCE_ALL), "faint named a source");
  CHECK(r->fallen_known && r->fallen_species == PIKACHU, "fallen");
}

/* An HP event of 0 is ambiguous and is counted, not stored; a text-only range stores nothing and makes no empty turn */
static void test_ignored_and_empty(void) {
  begin(false);
  pr_event be1[] = {switch_in(0, PIKACHU, 50, 50), hp_event(0, PIKACHU, 50, 50, 0)};
  gc_event q1[] = {gev(GC_EV_WAVE, 2, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 2)};
  drain(q1, 3, be1, 2);
  pr_event be2[] = {text(), text()};
  gc_event q2[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, 0, 2)};
  drain(q2, 2, be2, 2);
  const co_stats* st = co_stats_get();
  CHECK(st->ignored == 1, "ignored %u", st->ignored);
  CHECK(st->empty_ranges == 1 && st->batches == 1, "empty %u batches %u", st->empty_ranges, st->batches);
  const co_report* r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 1 && r->history.turns[0].n_events == 1, "one turn with one event");
  CHECK(!r->fallen_known, "nobody fell");
}

/* The same turn number in two waves are two turns; a 70-event turn keeps the newest 64; the sixth turn pushes the first out */
static void test_overflow_and_window(void) {
  begin(false);
  static pr_event big[70];
  for (int i = 0; i < 70; i++) big[i] = damage(0, PIKACHU, 1000 - i, 1000, 1);
  gc_event q[] = {gev(GC_EV_WAVE, 5, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 70)};
  drain(q, 3, big, 70);
  const co_stats* st = co_stats_get();
  CHECK(st->evicted == 6, "evicted %u", st->evicted);
  /* turn 1 of the next wave is not turn 1 again */
  pr_event one[] = {damage(0, PIKACHU, 500, 1000, 5)};
  gc_event q2[] = {gev(GC_EV_WAVE, 6, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q2, 3, one, 1);
  const co_report* r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 2, "two turns: %d", r->history.n_turns);
  CHECK(r->history.turns[0].battle_seq == 1 && r->history.turns[0].wave == 5 && r->history.turns[1].battle_seq == 2 && r->history.turns[1].wave == 6, "keys");
  CHECK(r->history.turns[0].n_events == 64 && r->history.turns[0].dropped == 6 && (r->history.turns[0].flags & CC_TF_EVICTED), "64 kept, 6 dropped, flagged");
  CHECK(r->history.turns[0].events[0].shown == 1 && r->history.turns[0].events[0].hp_after == 1000 - 6, "the oldest 6 went: first kept is the 7th");
  CHECK(r->history.turns[1].flags == 0 && r->history.turns[1].n_events == 1, "the next wave's turn is complete");

  begin(false);
  for (int turn = 1; turn <= 6; turn++) {
    pr_event e[] = {damage(0, PIKACHU, 100 - turn, 100, turn)};
    gc_event g[] = {gev(GC_EV_TURN, turn, 0), gev(GC_EV_BATTLE, 0, 1)};
    if (turn == 1) {
      gc_event w[] = {gev(GC_EV_WAVE, 1, 0), g[0], g[1]};
      drain(w, 3, e, 1);
    } else {
      drain(g, 2, e, 1);
    }
  }
  r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 5 && r->history.turns[0].turn == 2 && r->history.turns[4].turn == 6, "six turns keep the last five");
  CHECK(r->history.turns[0].flags == 0, "dropping a whole old turn flags nothing");
}

/* A new battle starts at turn 0 whatever the last one reached */
static void test_wave_boundary(void) {
  begin(false);
  pr_event a[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event q1[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_TURN, 3, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q1, 3, a, 1);
  pr_event b[] = {switch_in(0, PIKACHU, 50, 50)};
  gc_event q2[] = {gev(GC_EV_WAVE, 2, 0), gev(GC_EV_BATTLE, 0, 1)}; /* the next battle's intro: no TURN yet */
  drain(q2, 2, b, 1);
  const co_report* r = finish(false);
  if (!r || r->history.n_turns != 2) { CHECK(0, "two turns expected"); return; }
  CHECK(r->history.turns[0].turn == 3 && r->history.turns[0].battle_seq == 1 && r->history.turns[0].wave == 1, "first battle");
  CHECK(r->history.turns[1].turn == 0 && r->history.turns[1].battle_seq == 2 && r->history.turns[1].wave == 2, "the next battle starts at turn 0");
}

/* A replayed drain counts once; a drain that never arrived makes the next turn incomplete */
static void test_replay_and_skip(void) {
  begin(false);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event q[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q, 3, be, 1);
  co_observe(drain_no, q, 3, be, 1, false, false); /* the same drain again */
  co_observe(1, q, 3, be, 1, false, false);        /* an older number */
  const co_stats* st = co_stats_get();
  CHECK(st->drains == 1 && st->replayed_drains == 2 && st->batches == 1 && st->events_in == 1, "replay counted: %u/%u/%u/%u", st->drains, st->replayed_drains, st->batches, st->events_in);
  /* drains 2 and 3 never reach the observer; drain 4 does */
  drain_no += 2;
  pr_event be2[] = {damage(0, PIKACHU, 30, 50, 10)};
  gc_event q2[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q2, 2, be2, 1);
  CHECK(st->skipped_drains == 2, "skipped %u", st->skipped_drains);
  const co_report* r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 2 && r->history.turns[0].events[0].shown == 10 && r->history.turns[0].n_events == 1, "the replay added nothing");
  CHECK(r->history.turns[1].flags & CC_TF_SOURCE_LOSS, "the turn after the missing drains is flagged");
  CHECK(!r->untracked_loss, "the loss is on a stored turn");
}

/* The game says it lost events */
static void test_game_overflow(void) {
  begin(false);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event q[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  co_observe(++drain_no, q, 3, be, 1, true, false); /* battle_events_overflow */
  gc_event q2[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, 0, 1)};
  co_observe(++drain_no, q2, 2, be, 1, false, true); /* the queue overflowed once */
  gc_event q3[] = {gev(GC_EV_TURN, 3, 0), gev(GC_EV_BATTLE, 0, 1)};
  co_observe(++drain_no, q3, 2, be, 1, false, true); /* game-core never clears it: not counted again */
  const co_report* r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 3, "turns %d", r->history.n_turns);
  CHECK((r->history.turns[0].flags & CC_TF_SOURCE_LOSS) && (r->history.turns[1].flags & CC_TF_SOURCE_LOSS), "first two flagged");
  CHECK(r->history.turns[2].flags == 0, "the third is a full record");
  CHECK(r->stats.loss_flagged == 2, "flagged %u", r->stats.loss_flagged);

  /* a queue that came back full (GC_MAX_EVENTS events) may have dropped the next one */
  begin(false);
  static gc_event full[GC_MAX_EVENTS];
  for (int i = 0; i < GC_MAX_EVENTS; i++) full[i] = gev(GC_EV_MESSAGE, 0, 0);
  full[0] = gev(GC_EV_WAVE, 1, 0);
  full[1] = gev(GC_EV_TURN, 1, 0);
  full[2] = gev(GC_EV_BATTLE, 0, 1);
  drain(full, GC_MAX_EVENTS, be, 1);
  r = finish(false);
  CHECK(r && r->history.n_turns == 1 && (r->history.turns[0].flags & CC_TF_SOURCE_LOSS), "a full queue flags the turn");

  /* a loss with no range to put it on is remembered for the next one, and for the report if there is none */
  begin(false);
  co_observe(++drain_no, NULL, 0, NULL, 0, false, true);
  r = finish(false);
  CHECK(r && r->untracked_loss, "a loss with nowhere to go is reported");
}

/* Ranges that point outside the buffer, and events that cannot be represented */
static void test_bad_input(void) {
  begin(false);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10), damage(0, PIKACHU, 30, 50, 10)};
  gc_event w[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 5)};
  drain(w, 3, be, 2); /* 5 events from a buffer of 2 */
  gc_event q2[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, -1, 1), gev(GC_EV_BATTLE, 0, 0), gev(GC_EV_BATTLE, 1, 2)};
  drain(q2, 4, be, 2);
  const co_stats* st = co_stats_get();
  CHECK(st->bad_range == 4, "bad ranges %u", st->bad_range);
  CHECK(st->events_in == 0, "nothing observed from them: %u", st->events_in);
  const co_report* r = finish(false);
  if (!r) return;
  CHECK(r->history.n_turns == 2 && (r->history.turns[0].flags & CC_TF_SOURCE_LOSS) && (r->history.turns[1].flags & CC_TF_SOURCE_LOSS), "both turns say events are missing");
  CHECK(r->history.turns[0].n_events == 0 && r->history.turns[1].n_events == 0, "and nothing was invented");

  /* events the reducer cannot take: a negative hit, no species in the snapshot, a bad position, a status out of range */
  begin(false);
  pr_event bad[6];
  bad[0] = damage(0, PIKACHU, 40, 50, -3);
  bad[1] = damage(0, 0, 40, 50, 3);
  bad[2] = damage(0, PIKACHU, 40, 50, 3);
  bad[2].battler = 7;
  bad[3] = damage(0, PIKACHU, 40, 50, 3);
  bad[3].field[0].status = 200;
  bad[4] = damage(2, PIDGEY, 10, 40, 6); /* the one good one */
  bad[5] = switch_in(0, PIKACHU, 40, 50);
  bad[5].value = 9;
  gc_event q[] = {gev(GC_EV_WAVE, 4, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 6)};
  drain(q, 3, bad, 6);
  st = co_stats_get();
  CHECK(st->unrepresentable == 5 && st->events_in == 1, "unrepresentable %u, taken %u", st->unrepresentable, st->events_in);
  r = finish(false);
  CHECK(r && r->history.n_turns == 1 && r->history.turns[0].n_events == 1 && (r->history.turns[0].flags & CC_TF_SOURCE_LOSS), "the good one is kept and the turn says it is incomplete");

  /* an unknown snapshot HP is unknown, not zero */
  begin(false);
  pr_event nohp = damage(0, PIKACHU, 0, 0, 12);
  gc_event q4[] = {gev(GC_EV_WAVE, 4, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q4, 3, &nohp, 1);
  r = finish(false);
  CHECK(r && r->history.n_turns == 1 && r->history.turns[0].n_events == 1 && !(r->history.turns[0].events[0].known & CC_EK_HP_AFTER) && r->history.turns[0].events[0].shown == 12 && r->history.turns[0].flags == 0, "no snapshot HP");
}

/* Ranges before the wave or turn is known, and numbers out of range */
static void test_unkeyed(void) {
  begin(false);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event q[] = {gev(GC_EV_BATTLE, 0, 1)}; /* before any WAVE */
  drain(q, 1, be, 1);
  CHECK(co_stats_get()->unkeyed == 1 && co_stats_get()->batches == 0, "unkeyed");
  gc_event q2[] = {gev(GC_EV_WAVE, 70000, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q2, 3, be, 1);
  gc_event q3[] = {gev(GC_EV_WAVE, 0, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q3, 2, be, 1);
  gc_event q4[] = {gev(GC_EV_WAVE, 5, 0), gev(GC_EV_TURN, -1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q4, 3, be, 1);
  CHECK(co_stats_get()->unkeyed == 4, "unkeyed %u", co_stats_get()->unkeyed);
  const co_report* r = finish(false);
  CHECK(r && r->untracked_loss && r->history.n_turns == 0, "nothing stored, and the report says something was lost");
  /* a good turn after it carries the loss */
  begin(false);
  drain(q, 1, be, 1);
  gc_event ok[] = {gev(GC_EV_WAVE, 2, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(ok, 3, be, 1);
  r = finish(false);
  CHECK(r && !r->untracked_loss && r->history.n_turns == 1 && (r->history.turns[0].flags & CC_TF_SOURCE_LOSS), "the next stored turn carries the loss");
}

/* The reducer refuses a batch (a wave that goes backwards): nothing is stored for it, and the loss is not hidden */
static void test_reducer_refusal(void) {
  begin(false);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event a[] = {gev(GC_EV_WAVE, 9, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(a, 3, be, 1);
  gc_event b[] = {gev(GC_EV_WAVE, 4, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(b, 3, be, 1);
  const co_stats* st = co_stats_get();
  CHECK(st->rejected == 2 && st->last_status != CC_OK, "rejected %u status %d", st->rejected, st->last_status);
  const co_report* r = finish(false);
  CHECK(r && r->untracked_loss && r->history.n_turns == 1 && r->history.turns[0].wave == 9, "kept what was good, said what was lost");
}

/* Ending twice, observing after the end, starting again */
static void test_run_lifecycle(void) {
  begin(false);
  pr_event be[] = {switch_in(0, PIKACHU, 50, 50), faint(0, PIKACHU, 50)};
  gc_event q[] = {gev(GC_EV_WAVE, 3, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 2)};
  CHECK(co_report_get() == NULL, "no report before the run ends");
  drain(q, 3, be, 2);
  CHECK(co_report_get() == NULL, "no report while it runs");
  const co_report* r = finish(false);
  if (!r) return;
  static co_report copy;
  memcpy(&copy, r, sizeof copy);
  CHECK(co_finish(false, 99, 99), "a second finish is accepted");
  CHECK(memcmp(&copy, co_report_get(), sizeof copy) == 0, "a second finish changes nothing (not even with other numbers)");
  /* the screen replays after the end: ignored */
  gc_event q2[] = {gev(GC_EV_TURN, 2, 0), gev(GC_EV_BATTLE, 0, 2)};
  drain(q2, 2, be, 2);
  CHECK(memcmp(&copy, co_report_get(), sizeof copy) == 0, "events after the end change nothing");
  CHECK(co_stats_get()->drains == copy.stats.drains, "and are not counted");

  /* a new run: nothing of the old one is left, and the state starts again */
  co_begin_run(false);
  drain_no = 0;
  CHECK(co_report_get() == NULL, "no stale report");
  CHECK(co_stats_get()->drains == 0 && co_stats_get()->batches == 0 && co_stats_get()->last_status == CC_OK, "no stale counts");
  gc_event lone[] = {gev(GC_EV_BATTLE, 0, 1)};
  drain(lone, 1, be, 2);
  CHECK(co_stats_get()->unkeyed == 1, "no stale wave: a range before the new run's first WAVE is not placed on the old wave");
  gc_event q3[] = {gev(GC_EV_WAVE, 1, 0), gev(GC_EV_BATTLE, 0, 2)};
  drain(q3, 2, be, 2);
  r = finish(true);
  CHECK(r && r->outcome == CC_OUT_CLEAR && !r->fallen_known && r->history.n_turns == 1 && r->history.turns[0].battle_seq == 1 && r->history.turns[0].wave == 1, "the new run starts at battle 1");
  CHECK(r && r->history.turns[0].turn == 0, "and turn 0, not the old turn 2");
}

/* A run continued from a save: the history says observation began late */
static void test_resumed(void) {
  begin(true);
  pr_event be[] = {damage(0, PIKACHU, 40, 50, 10)};
  gc_event q[] = {gev(GC_EV_WAVE, 12, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 1)};
  drain(q, 3, be, 1);
  const co_report* r = finish(false);
  CHECK(r && r->began_mid && (r->history.flags & CC_HF_BEGAN_MID) && r->history.n_turns == 1 && r->history.turns[0].flags == 0, "resumed: the history is flagged, the turn is whole");
}

/* The clear and the foe's faint */
static void test_outcomes(void) {
  begin(false);
  pr_event be[] = {switch_in(0, PIKACHU, 50, 50), switch_in(2, PIDGEY, 40, 40), faint(2, PIDGEY, 40)};
  gc_event q[] = {gev(GC_EV_WAVE, 200, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 3)};
  drain(q, 3, be, 3);
  const co_report* r = finish(true);
  CHECK(r && r->outcome == CC_OUT_CLEAR && !r->fallen_known && r->history.turns[0].events[2].target_side == CC_SIDE_ENEMY, "a clear has no fallen Pokemon");
  begin(false);
  drain(q, 3, be, 3);
  r = finish(false);
  CHECK(r && r->outcome == CC_OUT_DEFEAT && !r->fallen_known, "a foe's faint is not the player's last to fall");
  /* the last of the player's faints, not the first */
  begin(false);
  pr_event two[] = {faint(0, PIKACHU, 50), switch_in(0, RATTATA, 30, 30), faint(0, RATTATA, 30)};
  gc_event q2[] = {gev(GC_EV_WAVE, 8, 0), gev(GC_EV_TURN, 1, 0), gev(GC_EV_BATTLE, 0, 3)};
  drain(q2, 3, two, 3);
  r = finish(false);
  CHECK(r && r->fallen_known && r->fallen_species == RATTATA, "the last one");
}

int main(void) {
  test_battle_in_drains();
  test_switch_and_reused_position();
  test_unknown_source();
  test_ignored_and_empty();
  test_overflow_and_window();
  test_wave_boundary();
  test_replay_and_skip();
  test_game_overflow();
  test_bad_input();
  test_unkeyed();
  test_reducer_refusal();
  test_run_lifecycle();
  test_resumed();
  test_outcomes();
  printf("observer_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
