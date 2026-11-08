/* The menus' image cache (see img_cache.h). */
#include "img_cache.h"

#include <stdio.h>
#include <string.h>

typedef enum { SLOT_FREE = 0, SLOT_LOADED, SLOT_MISSING, SLOT_FAILED } slot_state;

typedef struct {
  char key[64];
  rr_atlas* atlas;
  slot_state state;
  unsigned used;   /* the frame the atlas was last asked for */
  unsigned retry;  /* SLOT_FAILED: the frame a new try is allowed */
  unsigned tries;  /* SLOT_FAILED: how many tries failed in a row (the wait doubles, up to 16 times the first) */
} slot;

static imgc_ops g_ops;
static slot g_slots[IMGC_SLOTS];
static unsigned g_frame = 1;
static unsigned g_failures;
static char g_last_failure[64];

void imgc_init(const imgc_ops* ops)
{
  g_ops = *ops;
  memset(g_slots, 0, sizeof g_slots);
  g_frame = 1;
  g_failures = 0;
  g_last_failure[0] = 0;
}

void imgc_tick(void) { g_frame++; }

/* the icon sheets are the big ones and nobody keeps a frame pointer into them past the frame that drew it; the UI atlases ("items",
 * "ui/...") are looked up once by screens that hold on to their frames, so they are never freed to make room */
static bool evictable(const slot* s) { return s->state == SLOT_LOADED && !strncmp(s->key, "pokemon_icons_", 14) && s->used != g_frame; }

static void free_slot(slot* s)
{
  if (s->atlas) g_ops.unload(s->atlas);
  memset(s, 0, sizeof *s);
}

/* frees the least recently drawn evictable atlas; false when there is none */
static bool evict_one(void)
{
  slot* best = NULL;
  for (int i = 0; i < IMGC_SLOTS; i++)
    if (evictable(&g_slots[i]) && (!best || g_slots[i].used < best->used)) best = &g_slots[i];
  if (!best) return false;
  free_slot(best);
  return true;
}

int imgc_reclaim(void)
{
  int n = 0;
  while (evict_one()) n++;
  return n;
}

void imgc_clear(void)
{
  for (int i = 0; i < IMGC_SLOTS; i++) free_slot(&g_slots[i]);
}

const char* imgc_last_failure(unsigned* count)
{
  if (count) *count = g_failures;
  return g_failures ? g_last_failure : NULL;
}

/* a slot for a new key: a free one, else the least recently used one that holds no atlas worth keeping (a missing or failed key), else the
 * least recently used evictable atlas; NULL when every slot is busy this frame */
static slot* slot_for_new(void)
{
  slot* victim = NULL;
  for (int i = 0; i < IMGC_SLOTS; i++) {
    slot* s = &g_slots[i];
    if (s->state == SLOT_FREE) return s;
    const bool cheap = s->state == SLOT_MISSING || s->state == SLOT_FAILED;
    if (cheap && s->used != g_frame && (!victim || s->used < victim->used)) victim = s;
  }
  if (victim) {
    free_slot(victim);
    return victim;
  }
  if (!evict_one()) {
    /* nothing idle to give up: the old behaviour, drop it all (the screens load again what they draw) */
    imgc_clear();
  }
  for (int i = 0; i < IMGC_SLOTS; i++)
    if (g_slots[i].state == SLOT_FREE) return &g_slots[i];
  return NULL;
}

static rr_atlas* load_with_reclaim(const char* key)
{
  rr_atlas* a = g_ops.load(key);
  while (!a && g_ops.exists(key) && evict_one()) a = g_ops.load(key);
  return a;
}

rr_atlas* imgc_get(const char* key)
{
  slot* s = NULL;
  for (int i = 0; i < IMGC_SLOTS; i++)
    if (g_slots[i].state != SLOT_FREE && !strcmp(g_slots[i].key, key)) {
      s = &g_slots[i];
      break;
    }
  if (s) {
    if (s->state == SLOT_LOADED) {
      s->used = g_frame;
      return s->atlas;
    }
    if (s->state == SLOT_MISSING || g_frame < s->retry) return NULL;
    /* SLOT_FAILED, and the wait is over: another try below, in this slot */
  } else {
    s = slot_for_new();
    if (!s) return NULL;
    memset(s, 0, sizeof *s);
    snprintf(s->key, sizeof s->key, "%s", key);
    s->state = SLOT_FAILED; /* until the load says otherwise (evictions below must not see a half-made slot as loaded) */
  }
  s->used = g_frame;
  rr_atlas* a = load_with_reclaim(key);
  if (a) {
    s->atlas = a;
    s->state = SLOT_LOADED;
    return a;
  }
  if (g_ops.exists(key)) {
    s->state = SLOT_FAILED;
    s->retry = g_frame + (IMGC_RETRY_FRAMES << (s->tries < 4 ? s->tries : 4));
    s->tries++;
    g_failures++;
    snprintf(g_last_failure, sizeof g_last_failure, "%s", key);
  } else {
    s->state = SLOT_MISSING;
  }
  return NULL;
}
