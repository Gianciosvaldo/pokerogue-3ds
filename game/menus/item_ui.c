/* item_ui.c: the lookup of what the reward screens show of an item (icon, name, description), over gen/gm_item_ui.c. */
#include <string.h>

#include "gm_item_ui.h"

const gm_item_ui* gm_item_ui_find(const char* key, int arg) {
  if (!key) return NULL;
  /* the rows are sorted by key, then arg (the dump sorts them as strcmp and ints do) */
  int lo = 0, hi = gm_item_ui_count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    const gm_item_ui* r = &gm_item_ui_rows[mid];
    int c = strcmp(key, r->key);
    if (!c) c = arg < r->arg ? -1 : arg > r->arg;
    if (!c) return r;
    if (c < 0) hi = mid - 1;
    else lo = mid + 1;
  }
  return NULL;
}
