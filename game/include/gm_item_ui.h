/* What the reward screens show of each item (generated: gen/gm_item_ui.c, from game/tools/dump-item-ui.test.ts). */
#ifndef GM_ITEM_UI_H
#define GM_ITEM_UI_H

typedef struct {
  const char* key; /* modifierTypeInitObj key ("POTION", "TM_COMMON"...) */
  int arg;         /* a generator's argument (reward-core's gc_offer.arg), -1 for a plain item */
  const char* icon;  /* frame of the "items" atlas */
  const char* name;  /* English */
  const char* desc;  /* English; for the money items "{money}" stands for the amount, which depends on the run */
  int dynamic;    /* 1: desc has {money} */
} gm_item_ui;

extern const gm_item_ui gm_item_ui_rows[];
extern const int gm_item_ui_count;

/* The row for an offer (key, arg), or NULL. */
const gm_item_ui* gm_item_ui_find(const char* key, int arg);

#endif
