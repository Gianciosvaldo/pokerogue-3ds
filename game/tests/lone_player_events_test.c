/* A lone player's Pokemon in a double battle, in the battle screen's events (battle_events.c): BEV_PLAYER_CENTRE.
 * Upstream's FaintPhase moves the survivor to the centre at once; lorenzo wants it when the TURN is over (2026-11-15), so battle-core says
 * PR_EV_FIELD_CENTER where upstream moves it, and the converter holds the move to the end of the events:
 *   1  centre (value 1) in the middle of a batch: BEV_PLAYER_CENTRE comes after every event that follows it, last of the batch, and view->centred
 *   2  the same sign when the screen already shows the centre: nothing
 *   3  a partner back (value 0) in the same batch cancels a held centre; with the centre shown it is the way back, at once
 *   4  summon-time sign (id 1): at once, before the send-out that follows, the screen's instant move (b = 1)
 *   5  view->centred carries between batches (a second batch with the sign again gives nothing)
 * Host consistency of the converter, not an upstream comparison. */
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
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "lone player events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static int count(const bv_event* out, int n, bev_type t) {
  int c = 0;
  for (int i = 0; i < n; i++) c += out[i].type == t;
  return c;
}
static int find(const bv_event* out, int n, bev_type t) {
  for (int i = 0; i < n; i++)
    if (out[i].type == t) return i;
  return -1;
}

static void snap(pr_event* e) {
  e->field[0] = (pr_event_field){.species = SPECIES_ID_BULBASAUR, .hp = 100, .max_hp = 100, .visible = true, .party_slot = 0};
}
static pr_event centre(int value, int id) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = PR_EV_FIELD_CENTER;
  e.battler = -1;
  e.target = -1;
  e.value = value;
  e.id = id;
  snap(&e);
  return e;
}
static pr_event text(const char* key) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = PR_EV_TEXT;
  e.battler = -1;
  e.target = -1;
  e.msg.key = key;
  snap(&e);
  return e;
}
static void fresh(be_view* v, bool centred) {
  memset(v, 0, sizeof *v);
  v->present[0] = true;
  v->mon[0] = (bv_mon){.species = SPECIES_ID_BULBASAUR, .hp = 100, .max_hp = 100};
  strcpy(v->mon[0].name, "Species_1");
  v->centred = centred;
}

int main(void) {
  static bs_battle b;
  bs_begin(&b, BIOME_ID_PLAINS, 6, "lone_player_events");
  be_view v;
  bv_event out[32];
  pr_event ev[6];
  int n;

  /* 1: the faint's own sign, then text, then more text: the move is the last thing of the batch */
  fresh(&v, false);
  ev[0] = text("battle:test.a");
  ev[1] = centre(1, 0);
  ev[2] = text("battle:test.b");
  ev[3] = text("battle:test.c");
  n = be_convert_events(&b.bt, ev, 4, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 1);
  CHECK(find(out, n, BEV_PLAYER_CENTRE) == n - 1);
  CHECK(out[n - 1].a == 1 && out[n - 1].b == 0 && out[n - 1].battler == -1);
  CHECK(count(out, n, BEV_MESSAGE) == 3);
  CHECK(v.centred);

  /* 2: already centred: nothing, and it stays centred */
  fresh(&v, true);
  ev[0] = text("battle:test.a");
  ev[1] = centre(1, 0);
  n = be_convert_events(&b.bt, ev, 2, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 0 && v.centred);

  /* 5: the next batch has nothing to add */
  ev[0] = centre(1, 0);
  n = be_convert_events(&b.bt, ev, 1, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 0 && v.centred);

  /* 3a: a partner back before the end of the batch: the held centre is dropped, nothing is shown, nothing is moved */
  fresh(&v, false);
  ev[0] = centre(1, 0);
  ev[1] = text("battle:test.a");
  ev[2] = centre(0, 0);
  n = be_convert_events(&b.bt, ev, 3, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 0 && !v.centred);

  /* 3b: shown centred, the columns come back: at once, in place */
  fresh(&v, true);
  ev[0] = text("battle:test.a");
  ev[1] = centre(0, 0);
  ev[2] = text("battle:test.b");
  n = be_convert_events(&b.bt, ev, 3, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 1 && out[find(out, n, BEV_PLAYER_CENTRE)].a == 0);
  CHECK(find(out, n, BEV_PLAYER_CENTRE) < n - 1 && !v.centred);

  /* 3c: the columns, then the centre again in one batch: back and forth leaves the centre, shown once at the end */
  fresh(&v, true);
  ev[0] = centre(0, 0);
  ev[1] = text("battle:test.a");
  ev[2] = centre(1, 0);
  n = be_convert_events(&b.bt, ev, 3, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 2);
  CHECK(out[find(out, n, BEV_PLAYER_CENTRE)].a == 0 && out[n - 1].type == BEV_PLAYER_CENTRE && out[n - 1].a == 1 && v.centred);

  /* 4: a summon-time sign is at once (b = 1), in its place, and not held to the end of the batch */
  fresh(&v, false);
  ev[0] = centre(1, 1);
  ev[1] = text("battle:test.a");
  n = be_convert_events(&b.bt, ev, 2, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 1 && find(out, n, BEV_PLAYER_CENTRE) == 0 && out[0].a == 1 && out[0].b == 1 && v.centred);
  ev[0] = centre(0, 1);
  n = be_convert_events(&b.bt, ev, 1, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 1 && out[0].a == 0 && out[0].b == 1 && !v.centred);
  ev[0] = centre(0, 1);   /* already in the columns: nothing */
  n = be_convert_events(&b.bt, ev, 1, false, &v, out, 32);
  CHECK(count(out, n, BEV_PLAYER_CENTRE) == 0 && !v.centred);

  printf("Lone player events: %d checks (centre held to the end of the batch; cancelled by a partner; summon-time at once; no repeat)\n", checks);
  return 0;
}
