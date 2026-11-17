/* Kars's Perfect Being in the battle screen's events (battle_events.c): battle-core's PR_EV_PERFECT_BEING values 1 (he becomes the Ultimate Life
 * Form), 2 (a physical move did nothing) and 3 (a special move's damage was halved) each give one BEV_PERFECT with that value and battler.
 * Host consistency of the converter (custom content, no upstream counterpart). */
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
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "perfect being events FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static pr_event perfect(int value, int battler) {
  pr_event e;
  memset(&e, 0, sizeof e);
  e.type = PR_EV_PERFECT_BEING;
  e.battler = (int8_t)battler;
  e.target = -1;
  e.value = value;
  e.field[2] = (pr_event_field){.species = SPECIES_ID_SNORLAX, .hp = 100, .max_hp = 100, .visible = true, .party_slot = 5};
  return e;
}

int main(void) {
  static bs_battle b;
  bs_begin(&b, BIOME_ID_PLAINS, 6, "perfect_being_events");
  be_view v;
  bv_event out[32];
  pr_event ev[4];
  memset(&v, 0, sizeof v);
  /* each of battle-core's three Perfect Being events is one BEV_PERFECT with its value and battler, in order; the screen words the lines */
  ev[0] = perfect(1, 2);
  ev[1] = perfect(2, 2);
  ev[2] = perfect(3, 2);
  ev[3] = perfect(4, 2);   /* not a value battle-core sends: nothing */
  int n = be_convert_events(&b.bt, ev, 4, false, &v, out, 32);
  int k = 0;
  for (int i = 0; i < n; i++)
    if (out[i].type == BEV_PERFECT) {
      CHECK(k < 3 && out[i].a == k + 1 && out[i].battler == 2);
      k++;
    }
  CHECK(k == 3);
  printf("perfect being events: %d checks, 0 failed\n", checks);
  return 0;
}
