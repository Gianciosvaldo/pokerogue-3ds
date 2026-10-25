/* me_flow for the run driver's host tests: the question is answered at once, as the driver's old stand-in did (the first option the
 * run allows, with the first Pokemon that option's party list accepts; the first move), so the long seeded runs go through every
 * encounter. The real question, lists and texts are tested by me_screen_test. */
#include <string.h>

#include "me_flow.h"

void me_flow_intro(const gc_run* run, char* out, size_t n) {
  (void)run;
  if (n) out[0] = 0;
}

bool me_flow_open(const gc_run* run, void (*on_choice)(const me_choice* c, void* arg), void* arg) {
  const gc_screen* s = gc_run_screen(run);
  if (s->kind != GC_SCREEN_MYSTERY || s->n_options < 1) return false;
  me_choice c;
  memset(&c, 0, sizeof c);
  for (int i = 0; i < s->n_options && i < 8; i++)
    if (s->option_ok[i] && !(s->option_pick[i] && s->mystery_type == GC_ME_GLOBAL_TRADE_SYSTEM && i == 2)) {
      c.option = i;
      break;
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
