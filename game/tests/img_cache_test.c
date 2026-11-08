/* img_cache_test: the menus' image cache rules (menus/img_cache.c) over fake atlases and a fake pack with room for a few of them.
 *
 *   make img_cache_test && build/img_cache_test
 *
 * Checks: a hit is the same atlas; a key the pack lacks is remembered as missing and never loaded again; a load that fails for lack of room
 * frees the icon sheets not drawn this frame, least recently drawn first, and loads again; a sheet drawn this frame is never freed; the UI
 * atlases are never freed to make room; a failure with nothing to free is not remembered as missing (it is tried again later, with a wait
 * that grows) and is counted; a full table gives up one idle slot instead of dropping everything; imgc_reclaim frees the idle icon sheets
 * only. Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img_cache.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* the fake pack: the keys it holds, the room for atlases, and what was loaded and freed */
static const char* const PACK[] = {"pokemon_icons_1", "pokemon_icons_2", "pokemon_icons_3", "pokemon_icons_4", "pokemon_icons_5", "ui/bg", "items",
                                   "ui/a0", "ui/a1", "ui/a2", "ui/a3", "ui/a4", "ui/a5", "ui/a6", "ui/a7", "ui/a8", "ui/a9"};
static int g_room, g_loaded;      /* atlases that fit, atlases alive */
static int g_loads, g_unloads;
static char g_unloaded_keys[32][64];
static int n_unloaded_keys;
typedef struct { char key[64]; } fake_atlas;

static bool pack_has(const char* key)
{
  for (unsigned i = 0; i < sizeof PACK / sizeof PACK[0]; i++)
    if (!strcmp(PACK[i], key)) return true;
  return false;
}
static rr_atlas* fake_load(const char* key)
{
  if (!pack_has(key) || g_loaded >= g_room) return NULL;
  g_loads++;
  g_loaded++;
  fake_atlas* a = calloc(1, sizeof *a);
  snprintf(a->key, sizeof a->key, "%s", key);
  return (rr_atlas*)a;
}
static void fake_unload(rr_atlas* a)
{
  g_unloads++;
  g_loaded--;
  if (n_unloaded_keys < 32) snprintf(g_unloaded_keys[n_unloaded_keys++], 64, "%s", ((fake_atlas*)a)->key);
  free(a);
}
static const imgc_ops OPS = {fake_load, fake_unload, pack_has};

static void begin(int room)
{
  imgc_clear();
  imgc_init(&OPS);
  g_room = room;
  g_loaded = g_loads = g_unloads = n_unloaded_keys = 0;
}
static bool freed(const char* key)
{
  for (int i = 0; i < n_unloaded_keys; i++)
    if (!strcmp(g_unloaded_keys[i], key)) return true;
  return false;
}
static const char* name(rr_atlas* a) { return a ? ((fake_atlas*)a)->key : "(null)"; }

int main(void)
{
  /* a hit is the same atlas, loaded once */
  begin(10);
  rr_atlas* a = imgc_get("ui/bg");
  CHECK(a && !strcmp(name(a), "ui/bg") && imgc_get("ui/bg") == a && g_loads == 1, "one load, the same atlas");
  imgc_tick();
  CHECK(imgc_get("ui/bg") == a && g_loads == 1, "and next frame too");

  /* a key the pack lacks: NULL, remembered, never loaded again, not a failure */
  CHECK(imgc_get("pokemon_icons_9v") == NULL && imgc_get("pokemon_icons_9v") == NULL, "absent");
  for (int i = 0; i < 100; i++) imgc_tick(), imgc_get("pokemon_icons_9v");
  CHECK(g_loads == 1 && imgc_last_failure(NULL) == NULL, "an absent key is no failure and costs no load (%d loads)", g_loads);

  /* room for three: the fourth sheet frees the one drawn longest ago, then loads */
  begin(3);
  rr_atlas* s1 = imgc_get("pokemon_icons_1");
  imgc_tick();
  rr_atlas* s2 = imgc_get("pokemon_icons_2");
  imgc_tick();
  rr_atlas* s3 = imgc_get("pokemon_icons_3");
  imgc_tick();
  CHECK(s1 && s2 && s3 && g_loaded == 3, "three sheets fit");
  imgc_get("pokemon_icons_1");   /* sheet 1 is drawn again: sheet 2 is now the oldest */
  imgc_tick();
  rr_atlas* s4 = imgc_get("pokemon_icons_4");
  CHECK(s4 && !strcmp(name(s4), "pokemon_icons_4"), "the fourth sheet loads");
  CHECK(freed("pokemon_icons_2") && !freed("pokemon_icons_1") && !freed("pokemon_icons_3") && g_unloads == 1, "it freed sheet 2, the least recently drawn (%d freed)", g_unloads);
  CHECK(imgc_last_failure(NULL) == NULL, "that was no failure");
  CHECK(imgc_get("pokemon_icons_2") != NULL && g_unloads == 2, "sheet 2 comes back when drawn again, at the price of another one");

  /* a sheet drawn this frame is never freed: with everything drawn this frame the load fails, is not remembered as missing, and is counted */
  begin(2);
  imgc_tick();
  rr_atlas* b1 = imgc_get("pokemon_icons_1");
  rr_atlas* b2 = imgc_get("pokemon_icons_2");
  CHECK(b1 && b2, "two fit");
  CHECK(imgc_get("pokemon_icons_3") == NULL && g_unloads == 0, "no room and nothing idle: not there now, nothing freed");
  unsigned n = 0;
  const char* k = imgc_last_failure(&n);
  CHECK(n == 1 && k && !strcmp(k, "pokemon_icons_3"), "counted: %u %s", n, k ? k : "(none)");
  CHECK(imgc_get("pokemon_icons_3") == NULL && imgc_last_failure(&n) && n == 1, "asked again in the same frame: no new try, no new count");
  /* the wait: no try for IMGC_RETRY_FRAMES frames, then one, and the room that appeared is used */
  for (int i = 0; i < IMGC_RETRY_FRAMES - 1; i++) {
    imgc_tick();
    imgc_get("pokemon_icons_1");
    imgc_get("pokemon_icons_2");
  }
  g_room = 3;
  CHECK(imgc_get("pokemon_icons_3") == NULL, "still waiting one frame before the try");
  imgc_tick();
  imgc_get("pokemon_icons_1");
  imgc_get("pokemon_icons_2");
  rr_atlas* b3 = imgc_get("pokemon_icons_3");
  CHECK(b3 && !strcmp(name(b3), "pokemon_icons_3"), "after the wait it tries again and loads");
  CHECK(imgc_get("pokemon_icons_3") == b3, "and keeps it");

  /* the wait doubles with every failed try */
  begin(1);
  imgc_tick();
  imgc_get("pokemon_icons_1");
  int tries_seen = 0;
  unsigned last_fail = 0;
  for (int f = 0; f < 600; f++) {
    imgc_tick();
    imgc_get("pokemon_icons_1");   /* drawn every frame: never idle */
    imgc_get("pokemon_icons_2");   /* fails every time it is tried */
    unsigned c;
    imgc_last_failure(&c);
    if (c != last_fail) { tries_seen++; last_fail = c; }
  }
  CHECK(tries_seen == 5, "tries at frames 1, 31, 91, 211 and 451 (waits of 30, 60, 120, 240): %d in 600 frames", tries_seen);

  /* the UI atlases are never freed to make room, however idle */
  begin(2);
  imgc_tick();
  rr_atlas* u1 = imgc_get("ui/bg");
  rr_atlas* u2 = imgc_get("items");
  for (int i = 0; i < 5; i++) imgc_tick();
  CHECK(imgc_get("pokemon_icons_1") == NULL && g_unloads == 0, "no sheet to free: the UI atlases stay (%d freed)", g_unloads);
  CHECK(imgc_get("ui/bg") == u1 && imgc_get("items") == u2, "and are still the same atlases");

  /* a full table gives up one idle slot, not everything */
  begin(100);
  imgc_tick();
  rr_atlas* keep = imgc_get("items");
  for (int i = 0; i < 10; i++) {
    char key[16];
    snprintf(key, sizeof key, "ui/a%d", i);
    imgc_get(key);
  }
  for (int i = 0; i < IMGC_SLOTS - 11; i++) {
    char key[32];
    snprintf(key, sizeof key, "missing/%d", i);
    imgc_get(key);   /* absent keys fill the table */
  }
  imgc_tick();
  g_unloads = 0;
  imgc_get("pokemon_icons_1");   /* a new key with the table full */
  CHECK(g_unloads == 0, "an idle absent key made room, no atlas was freed (%d)", g_unloads);
  CHECK(imgc_get("items") == keep, "everything loaded is still there");

  /* imgc_reclaim: the idle sheets only */
  begin(10);
  imgc_tick();
  imgc_get("pokemon_icons_1");
  imgc_get("pokemon_icons_2");
  imgc_get("ui/bg");
  imgc_tick();
  imgc_get("pokemon_icons_2");   /* drawn in this frame */
  CHECK(imgc_reclaim() == 1 && freed("pokemon_icons_1") && !freed("pokemon_icons_2") && !freed("ui/bg"), "one idle sheet freed");
  CHECK(imgc_reclaim() == 0, "and nothing more");
  imgc_tick();
  CHECK(imgc_reclaim() == 1 && freed("pokemon_icons_2"), "a frame later the other one is idle too");

  /* clear frees all, and the cache starts over */
  imgc_clear();
  CHECK(g_loaded == 0, "everything freed");
  CHECK(imgc_get("ui/bg") != NULL, "and loads again");

  printf("img_cache_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
