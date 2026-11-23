/* me_flow for the run driver's host tests: the question is answered at once, as the driver's old stand-in did (the first option the
 * run allows, with the first Pokemon that option's party list accepts; the first move), so the long seeded runs go through every
 * encounter. The real question, lists and texts are tested by me_screen_test. */
#include <stdio.h>
#include <string.h>

#include "me_flow.h"

void me_flow_intro(const gc_run* run, char* out, size_t n) {
  (void)run;
  if (n) out[0] = 0;
}

/* Set by the run driver test: called as an encounter's screen is about to be answered (the fake answers at once, so this is the last moment before the run is stepped
 * again). It checks what the screen did to the run (the Safari Zone's IV Scanner question must not have touched the RNG streams) and settles the save the screen's question
 * is (an encounter's first question is upstream's save point). False leaves the screen unanswered: the driver drops the run and continues it from its slot. */
bool (*me_fake_before_open)(void);

bool me_flow_open(const gc_run* run, void (*on_choice)(const me_choice* c, void* arg), void* arg) {
  const gc_screen* s = gc_run_screen(run);
  if (s->kind != GC_SCREEN_MYSTERY) return false;
  if (me_fake_before_open && !me_fake_before_open()) return true;   /* the question stays up, as it would while the player reads it */
  if (s->n_options < 1) return false;
  me_choice c;
  memset(&c, 0, sizeof c);
  for (int i = 0; i < s->n_options && i < 8; i++)
    if (s->option_ok[i] && !(s->option_pick[i] && s->mystery_type == GC_ME_GLOBAL_TRADE_SYSTEM && i == 2)) {
      c.option = i;
      break;
    }
  if (s->mystery_type == GC_ME_JOBIN && s->me_stage == GC_ME_STAGE_JOBIN_ROUND) {
    /* Jobin's rounds: every one of the four moves in turn (a fixed rotation, no game stream), so Stand Tricks happen, the bot is caught cheating
     * now and then and his team is fought */
    static unsigned rot;
    c.option = (int)(((rot++ * 2654435761u) >> 16) % 4);
  }
  if (s->option_pick[c.option]) {
    for (int i = 0; i < run->n_party; i++)
      if (gc_mystery_member_ok(run, c.option, i)) {
        c.b = i;
        break;
      }
  } else {
    for (int i = 0; i < run->n_party; i++)
      if (run->party[i].b.hp > 0) {
        c.b = i;
        break;
      }
  }
  on_choice(&c, arg);
  return true;
}

const char* me_flow_ns(int type) {
  (void)type;
  return NULL;
}

/* the intro sprites on the field: one stand-in sprite for every encounter but the Pillar Men (whose silhouettes stay in the window) */
int me_flow_field_sprites(const gc_run* run, me_intro_sprite* out, int max) {
  if (run->mystery_type == GC_ME_PILLAR_MEN || max < 1) return 0;
  memset(out, 0, sizeof *out);
  snprintf(out->key, sizeof out->key, "images/mystery-encounters/fake");
  out->x = 164;
  out->y = 164;
  out->scale = 1;
  return 1;
}
void me_flow_set_darby_pose(int pose) { (void)pose; }
void me_flow_set_jobin_pose(int pose) { (void)pose; }
