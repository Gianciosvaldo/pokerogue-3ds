/* The final boss' second phase in the battle screen's events (battle_events.c): BEV_DOUBLES.
 * initFinalBossPhaseTwo turns the fight into a double: ToggleDoublePositionPhase(true) moves the lead to the left, then SummonPhase(1) sends out
 * the second Pokemon. The events carried no sign of the first, so the screen kept the single layout and drew the ally on the lead's place with its
 * HP box on the lead's (lorenzo, 2026-11-01). Battle-core now says so (PR_EV_DOUBLE_START, from the toggle phase initFinalBossPhaseTwo queued
 * and from no other) and the converter turns it into BEV_DOUBLES.
 *   1  hand-made events: the sign becomes one BEV_DOUBLES with no battler, in its place in the stream; the ally's summon after it is untouched
 *   2  real turns of battle-core at wave 200 of a Classic run: the hit that breaks the last shield of the Eternatus boss gives, in this order,
 *      the form change to Eternamax, BEV_DOUBLES, the summon of the second player Pokemon (battler 1), exactly once, and the fight is a double
 *   3  the same hit with one Pokemon left to send out: no toggle, no sign, no summon (availablePartyMembers.length > 1 is false)
 *   4  a boss that is not the Classic final boss (wave 190, or Eternatus already in its second phase) never gives the sign, whoever breaks its
 *      shields; neither does an ordinary switch, with the second Pokemon on the bench
 * Host consistency of the converter against battle-core's real engine, not an upstream comparison (the phase queue itself is differential-
 * tested: battle-core/tests/final-boss-test.c). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_events.h"
#include "battle_setup.h"
#include "battle_view.h"
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/events.h"
#include "pr_enums.h"

const char* i18n_language(void) { return "en"; }
const char* i18n_species_name(int id) {
  static char name[32];
  snprintf(name, sizeof name, "Species_%d", id);
  return name;
}
char* pak_read(const char* key, size_t* len) {
  (void)key;
  if (len) *len = 0;
  return NULL;
}
void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) { (void)ev; (void)n; (void)on_done; (void)arg; }

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "final boss doubles events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static int count(const bv_event* out, int n, bev_type t) {
  int c = 0;
  for (int i = 0; i < n; i++) c += out[i].type == t;
  return c;
}
static int find(const bv_event* out, int n, bev_type t, int battler) {
  for (int i = 0; i < n; i++)
    if (out[i].type == t && (battler < -1 || out[i].battler == battler)) return i;
  return -1;
}

static void hand_made(bs_battle* b) {
  be_view view;
  bv_event out[16];
  pr_event ev[3];
  memset(&view, 0, sizeof view);
  view.present[0] = true;
  view.mon[0] = (bv_mon){.species = SPECIES_ID_BULBASAUR, .hp = 100, .max_hp = 100};
  strcpy(view.mon[0].name, "Species_1");
  memset(ev, 0, sizeof ev);
  ev[0].type = PR_EV_DOUBLE_START;
  ev[0].battler = -1;
  ev[0].target = -1;
  ev[0].field[0] = (pr_event_field){.species = SPECIES_ID_BULBASAUR, .hp = 100, .max_hp = 100, .visible = true, .party_slot = 0};
  int n = be_convert_events(&b->bt, ev, 1, false, &view, out, 16);
  CHECK(n == 1 && out[0].type == BEV_DOUBLES && out[0].battler == -1);
  CHECK(view.present[0] && !view.present[2]);   /* the sign sends nobody out */
  /* a second sign (another toggle) is another event: the screen ignores the one it has already done */
  ev[1] = ev[0];
  n = be_convert_events(&b->bt, ev, 2, false, &view, out, 16);
  CHECK(n == 2 && out[0].type == BEV_DOUBLES && out[1].type == BEV_DOUBLES);
}

/* A Classic final boss fight: the player's party (first on the field), the Eternatus boss with `segments` shields, one big hit away from the
 * last. */
static pr_pokemon* boss_of(bs_battle* b) { return b->bt.field[2]; }
static void final_fight(bs_battle* b, int wave, int allowed, const char* seed, int boss_form) {
  int pm[] = {MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_THUNDERBOLT, MOVE_ID_SURF}, em[] = {MOVE_ID_SPLASH};
  bs_begin(b, BIOME_ID_END, wave, seed);
  bs_add(b, 0, SPECIES_ID_GARCHOMP, 100, pm, 4);
  if (allowed > 1) bs_add(b, 0, SPECIES_ID_BLAZIKEN, 100, pm, 4);
  if (allowed > 2) bs_add(b, 0, SPECIES_ID_ALAKAZAM, 100, pm, 4);
  pr_pokemon* boss = bs_add(b, 1, SPECIES_ID_ETERNATUS, 200, em, 1);
  boss->is_boss = true;
  boss->boss_segments = 2;
  boss->boss_segment_index = 1;
  boss->form = (uint8_t)boss_form;
  boss->hp = boss->stats[STAT_HP] * 55 / 100;   /* just above the line of the last shield (half of it): any hit that lands takes it down */
}

int main(void) {
  static bs_battle b;
  bs_begin(&b, BIOME_ID_END, 200, "final_boss_double_events");
  hand_made(&b);

  /* 2: the real thing, over a few seeds (the hit, the order, and that the fight is a double afterwards) */
  int fights = 0, phase_twos = 0, summons = 0, later_toggles = 0;
  for (int c = 0; c < 24; c++) {
    char seed[48];
    snprintf(seed, sizeof seed, "final_boss_double_%d", c);
    static bs_battle f;
    const int allowed = 2 + c % 2;
    final_fight(&f, 200, allowed, seed, 0);
    pr_pokemon* boss = boss_of(&f);
    CHECK(boss && boss->form == 0 && !f.bt.double_battle);
    bv_mon sp, se;
    be_mon(&f.bt, f.bt.field[0], &sp);
    be_mon(&f.bt, boss, &se);
    CHECK(se.boss && se.boss_segments == 2 && se.boss_segment_index == 1);
    fights++;
    bool two = false;
    for (int t = 0; t < 8 && !two && !bs_outcome(&f); t++) {
      if (bs_awaiting_player_pick(&f)) break;
      f.eng.n_events = f.eng.n_events_acknowledged = 0;
      bs_turn_fight(&f, c % 4);
      static bv_event out[PR_MAX_EVENTS * 2];
      be_view view;
      memset(&view, 0, sizeof view);
      view.present[0] = f.bt.field[0] != NULL;
      view.present[1] = true;
      view.mon[0] = sp;
      view.mon[1] = se;
      int n = be_convert_events(&f.bt, f.eng.events, f.eng.n_events, false, &view, out, PR_MAX_EVENTS * 2);
      if (!f.bt.double_battle) {
        CHECK(count(out, n, BEV_DOUBLES) == 0);   /* nothing before the last shield is down */
        if (f.bt.field[0]) sp.hp = f.bt.field[0]->hp;
        continue;
      }
      two = true;
      phase_twos++;
      /* the transition: exactly one sign, after the form change and before the ally is sent out */
      CHECK(count(out, n, BEV_DOUBLES) == 1);
      const int doubles = find(out, n, BEV_DOUBLES, -1);
      const int form = find(out, n, BEV_FORM_CHANGE, 2);
      const int ally = find(out, n, BEV_SUMMON, 1);
      CHECK(doubles >= 0 && form >= 0 && ally >= 0);
      CHECK(form < doubles && doubles < ally);
      CHECK(out[form].b == 1);   /* eternamax */
      CHECK(out[ally].mon && out[ally].mon->species != SPECIES_ID_GARCHOMP);   /* the second Pokemon of the party */
      summons++;
      CHECK(f.bt.field[1] && f.bt.field[1]->on_field && f.bt.field[1] == f.bt.party[0][1]);
      CHECK(boss_of(&f)->form == 1);
    }
    CHECK(two);   /* the first turn's hit breaks the last shield: every fight reaches the second phase */
    /* and the fight goes on as a double: a toggle that comes later (the lead faints and the ally takes its place: ToggleDoublePositionPhase
     * with nothing to do with the boss' transition) is not a transition, and says nothing */
    for (int t = 0; t < 4 && !bs_outcome(&f); t++) {
      if (bs_awaiting_player_pick(&f)) break;
      f.eng.n_events = f.eng.n_events_acknowledged = 0;
      bs_turn_fight(&f, c % 4);
      for (int i = 0; i < f.eng.n_events; i++) {
        CHECK(f.eng.events[i].type != PR_EV_DOUBLE_START);
        later_toggles += f.eng.events[i].type == PR_EV_FIELD_SWAP;
      }
    }
  }
  CHECK(fights == 24 && phase_twos == 24 && summons == 24);
  CHECK(later_toggles >= 4);   /* the seeds do reach toggles of the other kind: the check above is not vacuous */

  /* 3: nobody to send out: the fight is a double on paper, nothing moves on the screen */
  for (int c = 0; c < 6; c++) {
    char seed[48];
    snprintf(seed, sizeof seed, "final_boss_alone_%d", c);
    static bs_battle f;
    final_fight(&f, 200, 1, seed, 0);
    bv_mon sp, se;
    be_mon(&f.bt, f.bt.field[0], &sp);
    be_mon(&f.bt, boss_of(&f), &se);
    bool reached = false;
    for (int t = 0; t < 8 && !bs_outcome(&f); t++) {
      if (bs_awaiting_player_pick(&f)) break;
      f.eng.n_events = f.eng.n_events_acknowledged = 0;
      bs_turn_fight(&f, c % 4);
      static bv_event out[PR_MAX_EVENTS * 2];
      be_view view;
      memset(&view, 0, sizeof view);
      view.present[0] = f.bt.field[0] != NULL;
      view.present[1] = true;
      view.mon[0] = sp;
      view.mon[1] = se;
      int n = be_convert_events(&f.bt, f.eng.events, f.eng.n_events, false, &view, out, PR_MAX_EVENTS * 2);
      CHECK(count(out, n, BEV_DOUBLES) == 0 && find(out, n, BEV_SUMMON, 1) < 0);
      reached |= f.bt.double_battle;
    }
    CHECK(reached);
  }

  /* 4: not the Classic final boss: a boss at wave 190, the boss already in its second phase, a plain switch to the bench */
  for (int mode = 0; mode < 3; mode++)
    for (int c = 0; c < 6; c++) {
      char seed[48];
      snprintf(seed, sizeof seed, "final_boss_other_%d_%d", mode, c);
      static bs_battle f;
      final_fight(&f, mode == 0 ? 190 : 200, 2, seed, mode == 1 ? 1 : 0);
      bv_mon sp, se;
      be_mon(&f.bt, f.bt.field[0], &sp);
      be_mon(&f.bt, boss_of(&f), &se);
      for (int t = 0; t < (mode == 2 ? 1 : 4) && !bs_outcome(&f); t++) {
        if (bs_awaiting_player_pick(&f)) break;
        f.eng.n_events = f.eng.n_events_acknowledged = 0;
        if (mode == 2) bs_turn_switch(&f, 1);   /* (one turn: the next hit would be the final boss' own transition) */
        else bs_turn_fight(&f, c % 4);
        static bv_event out[PR_MAX_EVENTS * 2];
        be_view view;
        memset(&view, 0, sizeof view);
        view.present[0] = f.bt.field[0] != NULL;
        view.present[1] = true;
        view.mon[0] = sp;
        view.mon[1] = se;
        int n = be_convert_events(&f.bt, f.eng.events, f.eng.n_events, false, &view, out, PR_MAX_EVENTS * 2);
        CHECK(count(out, n, BEV_DOUBLES) == 0);
        for (int i = 0; i < f.eng.n_events; i++) CHECK(f.eng.events[i].type != PR_EV_DOUBLE_START);
      }
      CHECK(!f.bt.double_battle);
    }
  printf("Final boss doubles events: %d fights reach the second phase (%d allies sent out, %d later toggles say nothing), %d checks\n", fights, summons, later_toggles, checks);
  return 0;
}
