/* What the screen is told about Terastallization (TeraPhase, BattleInfo.updateTeraType): a real battle-core turn with the Tera command is run and
 * converted. Upstream's TeraPhase queues the message, plays CommonAnim.TERASTALLIZE and only then sets isTerastallized, so the animation's own
 * snapshot still shows false; the converter puts BEV_TERA right after the animation, and every later snapshot carries it. A Pokemon sent out
 * already Terastallized has it in its BEV_SUMMON mon, and a snapshot without it (the battle's end clears it) takes the icon away. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
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
  if (len) *len = 0;
  return NULL;
}
void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  (void)ev; (void)n; (void)on_done; (void)arg;
}

static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "tera visual FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static int count(const bv_event* ev, int n, bev_type t, int battler) {
  int c = 0;
  for (int i = 0; i < n; i++) c += ev[i].type == t && ev[i].battler == battler;
  return c;
}
static int index_of(const bv_event* ev, int n, bev_type t, int battler) {
  for (int i = 0; i < n; i++) if (ev[i].type == t && ev[i].battler == battler) return i;
  return -1;
}

int main(void) {
  static bs_battle b;
  static bv_event out[256];
  bs_begin(&b, BIOME_ID_GRASS, 5, "tera_visual_test_seed");
  int pm[] = {MOVE_ID_FLAMETHROWER, MOVE_ID_DRAGON_CLAW, MOVE_ID_AIR_SLASH, MOVE_ID_ROOST};
  int em[] = {MOVE_ID_TACKLE, MOVE_ID_GROWL, MOVE_ID_EARTHQUAKE, MOVE_ID_ROCK_SLIDE};
  pr_pokemon* me = bs_add(&b, 0, SPECIES_ID_CHARIZARD, 50, pm, 4);
  pr_pokemon* foe = bs_add(&b, 1, SPECIES_ID_GARCHOMP, 50, em, 4);
  me->tera_type = POKEMON_TYPE_DRAGON; /* the Tera type is not one of its own */
  foe->hp = foe->stats[STAT_HP] = 5000; /* it survives the turn */
  foe->tera_type = POKEMON_TYPE_GROUND;

  bv_mon shown_player = {0}, shown_enemy = {0};
  be_mon(&b.bt, me, &shown_player);
  be_mon(&b.bt, foe, &shown_enemy);
  CHECK(!shown_player.terastallized && !shown_enemy.terastallized);
  CHECK(shown_player.tera_type == POKEMON_TYPE_DRAGON && shown_enemy.tera_type == POKEMON_TYPE_GROUND);

  /* the player's Tera turn */
  bs_turn_tera(&b, 0);
  CHECK(me->terastallized);
  int anim = -1, tera_ev = -1;
  for (int i = 0; i < b.eng.n_events; i++) {
    const pr_event* e = &b.eng.events[i];
    if (e->type == PR_EV_COMMON_ANIM && e->id == COMMON_ANIM_TERASTALLIZE && e->battler == 0) anim = i;
  }
  CHECK(anim >= 0);
  /* the animation's snapshot: not yet Terastallized, but the type is there; the events after it show it */
  CHECK(!b.eng.events[anim].field[0].terastallized);
  CHECK(b.eng.events[anim].field[0].tera_type == POKEMON_TYPE_DRAGON);
  for (int i = anim + 1; i < b.eng.n_events; i++)
    if (b.eng.events[i].field[0].species) { tera_ev = i; break; }
  CHECK(tera_ev > anim && b.eng.events[tera_ev].field[0].terastallized);
  CHECK(b.eng.events[tera_ev].field[0].tera_type == POKEMON_TYPE_DRAGON);
  for (int i = 0; i < b.eng.n_events; i++) CHECK(!b.eng.events[i].field[2].terastallized);   /* the foe never did */

  int n = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out, 256);
  CHECK(count(out, n, BEV_TERA, 0) == 1);   /* once, not again with each later snapshot */
  CHECK(count(out, n, BEV_TERA, 2) == 0);
  int at = index_of(out, n, BEV_TERA, 0);
  CHECK(out[at].a == POKEMON_TYPE_DRAGON);
  /* right after the Terastallize animation, before the move that follows it */
  CHECK(at > 0 && out[at - 1].type == BEV_MOVE_ANIM && out[at - 1].b == BA_COMMON && out[at - 1].a == COMMON_ANIM_TERASTALLIZE &&
        out[at - 1].battler == 0);
  for (int i = 0; i < at; i++) CHECK(!(out[i].type == BEV_MOVE_ANIM && out[i].b == BA_MOVE && out[i].battler == 0));
  /* upstream plays the animation in TeraPhase.start and only queues the message (it comes after the phase, so after the icon) */
  int msg = -1;
  for (int i = 0; i < b.eng.n_events; i++)
    if (b.eng.events[i].type == PR_EV_TEXT && b.eng.events[i].msg.key && !strcmp(b.eng.events[i].msg.key, "battle:pokemonTerastallized")) msg = i;
  CHECK(msg > anim);

  /* the next turn starts from the screen as it stands: no second icon event */
  bv_mon now_player = {0};
  be_mon(&b.bt, me, &now_player);
  CHECK(now_player.terastallized && now_player.tera_type == POKEMON_TYPE_DRAGON);
  bs_turn_fight(&b, 0);
  n = be_convert(&b.bt, &b.eng, &now_player, &shown_enemy, out, 256);
  CHECK(count(out, n, BEV_TERA, 0) == 0);

  /* a screen that does not have the icon yet (a Pokemon whose snapshots carry it but the view does not) gets it once */
  n = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out, 256);
  CHECK(count(out, n, BEV_TERA, 0) == 1);
  CHECK(out[index_of(out, n, BEV_TERA, 0)].a == POKEMON_TYPE_DRAGON);

  /* the icon goes when the snapshots say it is gone (the battle's end clears isTerastallized) */
  {
    be_view view = {0};
    pr_event ev[1] = {{.type = PR_EV_TEXT, .battler = -1}};
    view.present[0] = true;
    view.mon[0] = (bv_mon){.species = SPECIES_ID_CHARIZARD, .hp = 20, .max_hp = 30, .terastallized = true, .tera_type = POKEMON_TYPE_DRAGON};
    ev[0].field[0] = (pr_event_field){.species = SPECIES_ID_CHARIZARD, .hp = 20, .max_hp = 30, .visible = true, .tera_type = POKEMON_TYPE_DRAGON};
    n = be_convert_events(&b.bt, ev, 1, false, &view, out, 256);
    CHECK(n >= 1 && out[0].type == BEV_TERA && out[0].battler == 0 && out[0].a == -1);
  }

  /* a Pokemon sent out already Terastallized (it was withdrawn and comes back) has the icon in its summon */
  {
    be_view view = {0};
    pr_event ev[1] = {{.type = PR_EV_SWITCH_IN, .battler = 0}};
    ev[0].field[0] = (pr_event_field){.species = SPECIES_ID_CHARIZARD, .hp = 20, .max_hp = 30, .visible = true, .terastallized = true,
                                      .tera_type = POKEMON_TYPE_FAIRY};
    n = be_convert_events(&b.bt, ev, 1, false, &view, out, 256);
    int s = index_of(out, n, BEV_SUMMON, 0);
    CHECK(s >= 0 && out[s].mon && out[s].mon->terastallized && out[s].mon->tera_type == POKEMON_TYPE_FAIRY);
    CHECK(count(out, n, BEV_TERA, 0) == 0);
  }

  /* the enemy Terastallizes too (a trainer's Pokemon): the icon is for its own side */
  {
    bs_begin(&b, BIOME_ID_GRASS, 5, "tera_visual_test_enemy");
    me = bs_add(&b, 0, SPECIES_ID_CHARIZARD, 50, pm, 4);
    foe = bs_add(&b, 1, SPECIES_ID_GARCHOMP, 50, em, 4);
    foe->tera_type = POKEMON_TYPE_STELLAR;
    me->hp = me->stats[STAT_HP] = 5000;
    be_mon(&b.bt, me, &shown_player);
    be_mon(&b.bt, foe, &shown_enemy);
    /* the foe's TeraPhase (a trainer's instant Tera runs it at the start of the turn) */
    memset(&b.eng.events, 0, sizeof b.eng.events);
    b.eng.n_events = 0;
    int ph = pr_phase_new(&b.eng, PH_TERA, 2);
    CHECK(ph >= 0);
    b.eng.pool[ph].pokemon = foe;
    b.eng.current = ph;
    pr_phase_run(&b.bt, &b.eng, ph);
    CHECK(foe->terastallized);
    n = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out, 256);
    CHECK(count(out, n, BEV_TERA, 2) == 1 && count(out, n, BEV_TERA, 0) == 0);
    CHECK(out[index_of(out, n, BEV_TERA, 2)].a == POKEMON_TYPE_STELLAR);
  }

  printf("tera_visual_test PASSED (%d checks)\n", checks);
  return 0;
}
