/* The words and art of a custom achievement's bar (see achv_toast.h): game-core's achievement table, which the queue itself does not need. */
#include <stdio.h>
#include <string.h>

#include "achv_toast.h"
#include "gc/achv.h"

static const char* const VOUCHER_ICON[4] = {"coupon", "pair_of_tickets", "mystic_ticket", "golden_mystic_ticket"};

bool atoast_text_of(int id, atoast_text* o) {
  if (!o || id < 0 || id >= GC_ACHV_COUNT) return false;
  const gc_achv_def* d = &gc_achv_defs[id];
  memset(o, 0, sizeof *o);
  snprintf(o->title, sizeof o->title, "%s", d->name);
  snprintf(o->desc, sizeof o->desc, "%s\nReward: %s", d->desc, gc_achv_reward_name(d));
  /* art by reward: a voucher is its own icon in its own tier (as upstream's voucher bars are); an egg is the Lucky Egg, the Golden Egg from a
   * Legendary one, in the tier above its own (a Common egg is a bar of tier 1); an egg with a guaranteed shiny is the highest tier */
  switch (d->reward) {
    case GC_AREW_VOUCHER:
      o->tier = d->arg < 4 ? d->arg : 0;
      o->icon = VOUCHER_ICON[o->tier];
      break;
    case GC_AREW_EGG:
      o->tier = d->arg < 4 ? d->arg + 1 : 1;
      o->icon = d->arg >= 3 ? "golden_egg" : "lucky_egg";
      break;
    default:
      o->tier = 4;
      o->icon = "golden_egg";
      break;
  }
  return true;
}

void atoast_push_achv(int id) {
  atoast_text t;
  if (!atoast_text_of(id, &t)) return;
  const tv_info info = {t.title, t.desc, t.score, t.icon, t.tier};
  atoast_push(&info, true);
}
