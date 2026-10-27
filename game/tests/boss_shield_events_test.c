/* A boss's shields in the battle screen's events (battle_events.c): BEV_BOSS_SHIELD, and the boss fields of a summon.
 * Battle-core keeps EnemyPokemon.bossSegmentIndex, and a hit that clears segments changes it before the hit's DAMAGE event;
 * its events carried only HP, so the screen could not draw the HP bar's dividers. The event snapshot now carries
 * boss_segments and boss_segment_index; the converter emits BEV_BOSS_SHIELD before the event that shows the lowered index.
 *   1  hand-made events: the shield comes before the damage that broke it, once for two broken at once, never for a non-boss, never up
 *   2  a summon takes the snapshot's segments and index; the catch-up after the events names a shield they did not
 *   3  real turns of battle-core (hits, crits, an enemy that heals): over many seeded boss fights the shield events are exactly the
 *      distinct values the engine's own index went through, in order, each one lower than the last, and what the screen shows
 *      after every turn is the engine's boss_segment_index (a heal never puts one back)
 * Host consistency of the converter against battle-core's real damage code, not an upstream comparison (the shield index itself is
 * battle-core's, already differential-tested: bosssegmenttest). */
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
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "boss shield events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static pr_event dmg(int type, int hp, int segments, int index) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = (uint8_t)type;
  e.battler = 2;
  e.target = -1;
  e.field[2] = (pr_event_field){.species = 143, .hp = hp, .max_hp = 100, .visible = true, .boss_segments = (uint8_t)segments,
                                .boss_segment_index = (uint8_t)index, .party_slot = 0};
  return e;
}

static void redraw_after_shield(bs_battle *b) {
  const int kinds[]={PR_EV_TRANSFORM,PR_EV_ILLUSION_BREAK};
  for(unsigned k=0;k<sizeof kinds/sizeof *kinds;k++) {
    be_view v={0};v.present[1]=true;
    v.mon[1]=(bv_mon){.species=143,.hp=100,.max_hp=100,.boss=true,.boss_segments=2,.boss_segment_index=1};
    pr_event ev[2]={dmg(PR_EV_DAMAGE,50,2,0),dmg(kinds[k],50,2,0)};
    ev[1].field[2].species=25;
    bv_event out[8];int n=be_convert_events(&b->bt,ev,2,false,&v,out,8),shield=0,redraw=0;
    for(int i=0;i<n;i++) {
      if(out[i].type==BEV_BOSS_SHIELD){CHECK(out[i].a==0);shield++;}
      if(out[i].type==BEV_TRANSFORM||out[i].type==BEV_ILLUSION) {
        CHECK(shield==1&&out[i].mon&&out[i].mon->boss);
        CHECK(out[i].mon->boss_segments==2&&out[i].mon->boss_segment_index==0);
        CHECK(out[i].mon->hp==50);redraw++;
      }
    }
    CHECK(shield==1&&redraw==1);
  }
}
static void hand_made(bs_battle* b) {
  be_view view;
  bv_event out[16];
  pr_event ev[6];
  memset(&view, 0, sizeof view);
  view.present[1] = true;
  view.mon[1] = (bv_mon){.species = 143, .hp = 100, .max_hp = 100, .boss = true, .boss_segments = 4, .boss_segment_index = 3};
  strcpy(view.mon[1].name, "Species_143");
  /* 1: a hit that breaks one shield (3 -> 2), a hit that breaks none, a heal, a hit that breaks two at once (2 -> 0): the shield comes
   * first, once, then the damage; none for the hit that broke none */
  ev[0] = dmg(PR_EV_DAMAGE, 70, 4, 2);
  ev[1] = dmg(PR_EV_DAMAGE, 60, 4, 2);
  ev[2] = dmg(PR_EV_HP, 80, 4, 2);
  ev[3] = dmg(PR_EV_DAMAGE, 20, 4, 0);
  int n = be_convert_events(&b->bt, ev, 4, false, &view, out, 16);
  CHECK(n == 6);
  CHECK(out[0].type == BEV_BOSS_SHIELD && out[0].battler == 2 && out[0].a == 2);
  CHECK(out[1].type == BEV_DAMAGE && out[1].a == 70);
  CHECK(out[2].type == BEV_DAMAGE && out[2].a == 60);
  CHECK(out[3].type == BEV_HEAL && out[3].a == 80);
  CHECK(out[4].type == BEV_BOSS_SHIELD && out[4].battler == 2 && out[4].a == 0);
  CHECK(out[5].type == BEV_DAMAGE && out[5].a == 20);
  /* a snapshot that shows the index back up (a stale or foreign one) never raises the shield: nothing is emitted */
  view.mon[1].boss_segment_index = 1;
  ev[0] = dmg(PR_EV_DAMAGE, 50, 4, 3);
  n = be_convert_events(&b->bt, ev, 1, false, &view, out, 16);
  CHECK(n == 1 && out[0].type == BEV_DAMAGE);
  /* a Pokemon that is no boss never gets one, whatever its index says */
  memset(&view, 0, sizeof view);
  view.present[1] = true;
  view.mon[1] = (bv_mon){.species = 143, .hp = 100, .max_hp = 100};
  strcpy(view.mon[1].name, "Species_143");
  ev[0] = dmg(PR_EV_DAMAGE, 50, 0, 0);
  n = be_convert_events(&b->bt, ev, 1, false, &view, out, 16);
  CHECK(n == 1 && out[0].type == BEV_DAMAGE);
  /* the player's side: its own battler, no shield for it */
  memset(&view, 0, sizeof view);
  view.present[0] = true;
  view.mon[0] = (bv_mon){.species = 143, .hp = 100, .max_hp = 100};
  strcpy(view.mon[0].name, "Species_143");
  ev[0] = dmg(PR_EV_DAMAGE, 50, 0, 0);
  ev[0].battler = 0;
  ev[0].field[0] = ev[0].field[2];
  memset(&ev[0].field[2], 0, sizeof ev[0].field[2]);
  n = be_convert_events(&b->bt, ev, 1, false, &view, out, 16);
  CHECK(n == 1 && out[0].type == BEV_DAMAGE && out[0].battler == 0);
}

static void summon_and_catch_up(void) {
  static bs_battle b;
  int pm[] = {MOVE_ID_TACKLE}, em[] = {MOVE_ID_SPLASH};
  bs_begin(&b, BIOME_ID_GRASS, 10, "boss_shield_summon");
  bs_add(&b, 0, SPECIES_ID_BLAZIKEN, 50, pm, 1);
  pr_pokemon* boss = bs_add(&b, 1, SPECIES_ID_SNORLAX, 40, em, 1);
  boss->is_boss = true;
  boss->boss_segments = 4;
  boss->boss_segment_index = 3;
  bv_mon shown;
  be_mon(&b.bt, boss, &shown);
  CHECK(shown.boss && shown.boss_segments == 4 && shown.boss_segment_index == 3);
  /* 2: a summon (a trainer's next Pokemon) takes the snapshot's segments and index, not the live ones */
  be_view view;
  bv_event out[16];
  pr_event ev;
  memset(&view, 0, sizeof view);
  memset(&ev, 0, sizeof ev);
  ev.type = PR_EV_SWITCH_IN;
  ev.battler = 2;
  ev.target = -1;
  ev.value = 0;
  ev.field[2] = (pr_event_field){.species = SPECIES_ID_SNORLAX, .hp = boss->stats[STAT_HP], .max_hp = boss->stats[STAT_HP], .visible = true,
                                 .party_slot = 0, .boss_segments = 4, .boss_segment_index = 1};
  int n = be_convert_events(&b.bt, &ev, 1, false, &view, out, 16);
  bool summoned = false;
  for (int i = 0; i < n; i++)
    if (out[i].type == BEV_SUMMON && out[i].battler == 2) {
      summoned = true;
      CHECK(out[i].mon->boss && out[i].mon->boss_segments == 4 && out[i].mon->boss_segment_index == 1);
    }
  CHECK(summoned);
  /* the catch-up after events that never showed a shield break: the battle as it stands names it (settled) */
  memset(&view, 0, sizeof view);
  view.present[1] = true;
  view.mon[1] = shown;
  boss->boss_segment_index = 1;
  n = be_convert_events(&b.bt, NULL, 0, true, &view, out, 16);
  CHECK(n == 1 && out[0].type == BEV_BOSS_SHIELD && out[0].battler == 2 && out[0].a == 1);
  /* not settled (the run went on after the events): nothing is invented */
  n = be_convert_events(&b.bt, NULL, 0, false, &view, out, 16);
  CHECK(n == 0);
}

static uint32_t rng_state = 12345;
static int rnd(int n) {
  rng_state = rng_state * 1664525u + 1013904223u;
  return (int)((rng_state >> 8) % (uint32_t)n);
}

int main(void) {
  static bs_battle b;
  bs_begin(&b, BIOME_ID_GRASS, 10, "boss_shield_events_test");
  hand_made(&b);redraw_after_shield(&b);
  summon_and_catch_up();

  /* 3: real battles. Player Pokemon of mid level with their strongest moves against a tanky boss of 2..7 segments whose moves heal or
   * do nothing, so the fight lasts and its shields break in every way the damage code has: one at a time, several at once, with
   * critical hits, after a heal. */
  static const int foes[] = {SPECIES_ID_SNORLAX, SPECIES_ID_BLISSEY, SPECIES_ID_STEELIX};
  static const int mine[] = {SPECIES_ID_BLAZIKEN, SPECIES_ID_GARCHOMP, SPECIES_ID_ALAKAZAM};
  const int attack[] = {MOVE_ID_BLAZE_KICK, MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_THUNDERBOLT, MOVE_ID_SLASH, MOVE_ID_SURF};
  int fights = 0, turns = 0, breaks = 0, multi = 0, heals_after_break = 0, ended_boss_down = 0, boss_seen = 0;
  for (int c = 0; c < 400; c++) {
    char seed[40];
    snprintf(seed, sizeof seed, "boss_shield_fight_%d", c);
    static bs_battle fight;
    bs_begin(&fight, BIOME_ID_GRASS, 10 + 10 * rnd(14), seed);
    int pmv[4], emv[3] = {MOVE_ID_RECOVER, MOVE_ID_SPLASH, MOVE_ID_TACKLE};
    for (int k = 0; k < 4; k++) pmv[k] = attack[(c + k * 5) % 6];
    int level = 25 + rnd(40);
    pr_pokemon* me = bs_add(&fight, 0, mine[rnd(3)], level, pmv, 4);
    pr_pokemon* boss = bs_add(&fight, 1, foes[rnd(3)], level + 15 + rnd(40), emv, 3);
    const int segments = 2 + rnd(6);
    boss->is_boss = true;
    boss->boss_segments = (uint8_t)segments;
    boss->boss_segment_index = (uint8_t)(segments - 1);
    CHECK(me && boss);
    bv_mon sp, se;
    be_mon(&fight.bt, me, &sp);
    be_mon(&fight.bt, boss, &se);
    CHECK(se.boss && se.boss_segments == segments && se.boss_segment_index == segments - 1);
    boss_seen++;
    fights++;
    bool down = false;
    for (int t = 0; t < 60 && !down && !bs_outcome(&fight); t++) {
      if (bs_awaiting_player_pick(&fight)) break;
      fight.eng.n_events = fight.eng.n_events_acknowledged = 0;
      const int before = fight.bt.field[2] ? fight.bt.field[2]->boss_segment_index : -1;
      bs_turn_fight(&fight, rnd(4));
      turns++;
      /* what the engine's own index went through, in its events, for the foe's slot */
      int engine_values[PR_MAX_EVENTS + 1], ne = 0, last = before, hp_before_heal = -1;
      for (int i = 0; i < fight.eng.n_events; i++) {
        const pr_event_field* f = &fight.eng.events[i].field[2];
        if (!f->boss_segments || f->fainted) continue;
        CHECK(f->boss_segment_index <= last); /* it only ever counts down */
        if (f->boss_segment_index < last) {
          engine_values[ne++] = f->boss_segment_index;
          if (last - f->boss_segment_index > 1) multi++;
          last = f->boss_segment_index;
        }
        if (fight.eng.events[i].type == PR_EV_HP && fight.eng.events[i].battler == 2 && last < segments - 1) hp_before_heal = f->hp;
      }
      if (hp_before_heal >= 0) heals_after_break++;
      static bv_event out[PR_MAX_EVENTS * 2];
      int n = be_convert(&fight.bt, &fight.eng, &sp, &se, out, PR_MAX_EVENTS * 2);
      int shields = 0, shown = before;
      for (int i = 0; i < n; i++) {
        if (out[i].type == BEV_BOSS_SHIELD) {
          CHECK(out[i].battler == 2);
          CHECK(out[i].a < shown);                       /* each one lower than the last */
          CHECK(shields < ne && out[i].a == engine_values[shields]); /* exactly the values the engine went through, in order */
          shown = out[i].a;
          shields++;
          breaks++;
        }
        if (out[i].battler == 0 && (out[i].type == BEV_DAMAGE || out[i].type == BEV_HEAL)) sp.hp = out[i].a;
        if (out[i].battler == 2 && (out[i].type == BEV_DAMAGE || out[i].type == BEV_HEAL)) se.hp = out[i].a;
      }
      /* the boss fainted in this turn: its last events carry no index (the engine's own boss_segment_index is whatever the killing hit left) */
      down = fight.bt.field[2] == NULL || fight.bt.field[2]->hp <= 0;
      if (!down) {
        CHECK(shields == ne);
        CHECK(shown == fight.bt.field[2]->boss_segment_index); /* what the screen shows is the engine's */
        se.boss_segment_index = shown;
      }
      sp.hp = fight.bt.field[0] ? fight.bt.field[0]->hp : 0;
      if (down) ended_boss_down++;
    }
  }
  printf("Boss shield events: %d fights, %d turns, %d shield breaks shown (%d hits broke two or more at once, %d turns with a heal after a break), "
         "%d bosses downed, %d checks\n", fights, turns, breaks, multi, heals_after_break, ended_boss_down, checks);
  /* coverage the seeds must keep: shields were broken one at a time and several at once, healed after, and bosses were beaten */
  CHECK(fights == 400 && boss_seen == 400);
  CHECK(breaks >= 200);
  CHECK(multi >= 3);
  CHECK(heals_after_break >= 3);
  CHECK(ended_boss_down >= 20);
  return 0;
}
