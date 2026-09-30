/* Host check of switching through the stand-in battle: a switch turn, then a faint switch.
 * build/switchtest path/to/pokerogue/locales/en */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_setup.h"
#include "pr/events.h"
#include "pr/text.h"
#include "pr_data.h"
#include "pr_enums.h"

static const char* g_loc;
static const char* get(void* ctx, const char* file, int fb, size_t* len) {
  static char* cache[64];
  static char names[64][48];
  static int n;
  if (fb) return NULL;
  for (int i = 0; i < n; i++)
    if (!strcmp(names[i], file)) return *len = strlen(cache[i]), cache[i];
  char p[512];
  snprintf(p, sizeof p, "%s/%s.json", g_loc, file);
  FILE* f = fopen(p, "rb");
  if (!f || n == 64) return NULL;
  fseek(f, 0, SEEK_END);
  long l = ftell(f);
  fseek(f, 0, SEEK_SET);
  cache[n] = calloc(l + 1, 1);
  if (fread(cache[n], 1, l, f) != (size_t)l) l = 0;
  fclose(f);
  snprintf(names[n], 48, "%s", file);
  *len = l;
  return cache[n++];
}

static void dump(bs_battle* b, const char* what) {
  static const char* T[] = {"NONE", "TEXT", "MOVE_ANIM", "CHARGE_ANIM", "COMMON_ANIM", "DAMAGE", "HP", "STAT_STAGES",
                            "ABILITY_SHOW", "ABILITY_HIDE", "POKEMON_ANIM", "FAINT", "SWITCH_OUT", "SWITCH_IN",
                            "SHINY_SPARKLE"};
  pr_text_source src = {get, NULL, "en"};
  printf("--- %s (stop %d, pick %d)\n", what, b->eng.stop, bs_awaiting_player_pick(b));
  for (int i = 0; i < b->eng.n_events; i++) {
    const pr_event* e = &b->eng.events[i];
    char t[256] = "";
    if (e->type == PR_EV_TEXT && e->msg.key) pr_msg_format(&e->msg, &src, t, sizeof t);
    printf("  %-12s b=%d v=%d %s\n", e->type < 15 ? T[e->type] : "?", e->battler, e->value, t);
  }
}

/* A faint stopped the turn at the player's SwitchPhase: send out the last healthy benched Pokemon. */
static void pick_if_asked(bs_battle* b) {
  if (!bs_awaiting_player_pick(b)) return;
  int slot = -1;
  for (int i = 0; i < b->bt.n_party[0]; i++)
    if (b->bt.party[0][i]->hp > 0 && !b->bt.party[0][i]->on_field) slot = i;
  bs_pick(b, slot);
  dump(b, "picked");
}

int main(int argc, char** argv) {
  g_loc = argc > 1 ? argv[1] : ".";
  bs_battle* b = calloc(1, sizeof *b);
  bs_begin(b, BIOME_ID_GRASS, 5, "switchtest");
  int pm[] = {MOVE_ID_TACKLE}, lm[] = {MOVE_ID_SURF}, gm[] = {MOVE_ID_SHADOW_BALL},
      em[] = {MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW};
  bs_add(b, 0, SPECIES_ID_BLAZIKEN, 20, pm, 1);
  bs_add(b, 0, SPECIES_ID_LAPRAS, 20, lm, 1);
  bs_add(b, 0, SPECIES_ID_GENGAR, 20, gm, 1);
  bs_add(b, 1, SPECIES_ID_GARCHOMP, 80, em, 2);
  bs_turn_switch(b, 1);
  dump(b, "switch to slot 1");
  printf("field[0] = species %d, party[0][0] = %d\n", b->bt.field[0]->species, b->bt.party[0][0]->species);
  pick_if_asked(b);
  for (int t = 0; t < 6 && !bs_outcome(b); t++) {
    bs_turn_fight(b, 0);
    dump(b, "fight");
    pick_if_asked(b);
  }
  printf("outcome %d\n", bs_outcome(b));
  return bs_outcome(b) == 2 ? 0 : 1; /* a level 80 Garchomp beats the level 20 party */
}
