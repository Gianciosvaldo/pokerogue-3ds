/* egglist_model_test: the Egg List's model (source/egglist_model.c, source/scroll_grid.c) against upstream's own EggListUiHandler.
 *
 *   make egglist_model_test && build/egglist_model_test fixtures/egg-list.native.txt.gz
 *
 * The fixture is made by egglist.producer.test.ts in the pinned upstream checkout (Node 22, TZ=UTC): the real handler, headless, over random egg
 * lists (1 to 150 eggs, every tier, source type and hatch-wave band, Manaphy and legacy eggs, featured-legendary eggs, dates over five years).
 * Per egg this builds the same gc_egg, runs egm_describe and compares the five texts (frame key, name, date, hatch message, type line); per
 * navigation step (300 random UP / DOWN / LEFT / RIGHT per list, from a freshly opened list) it compares the result of the press, the cursor, the
 * scroll position, the panel (name, date) and every icon of the grid. Also unit checks of the date format, the Manaphy rule and the grid edges.
 * Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "egglist_model.h"

static int checks, failed;
#define CHECK(cond, ...)                               \
  do {                                                 \
    checks++;                                          \
    if (!(cond)) {                                     \
      failed++;                                        \
      if (failed < 30) {                               \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                           \
        printf("\n");                                  \
      }                                                \
    }                                                  \
  } while (0)

static egm_texts T;
static char names[2100][64];
static const char* species_name(int id, void* ud) {
  (void)ud;
  return id >= 0 && id < 2100 && names[id][0] ? names[id] : "?";
}

static gc_egg eggs[200];
static int n_eggs;
static egl_state L;

static void set_text(const char* key, const char* text) {
  if (!strcmp(key, "egg")) snprintf(T.egg, sizeof T.egg, "%s", text);
  else if (!strcmp(key, "defaultTier")) snprintf(T.tier[0], sizeof T.tier[0], "%s", text);
  else if (!strcmp(key, "greatTier")) snprintf(T.tier[1], sizeof T.tier[1], "%s", text);
  else if (!strcmp(key, "ultraTier")) snprintf(T.tier[2], sizeof T.tier[2], "%s", text);
  else if (!strcmp(key, "masterTier")) snprintf(T.tier[3], sizeof T.tier[3], "%s", text);
  else if (!strcmp(key, "manaphyTier")) snprintf(T.tier[4], sizeof T.tier[4], "%s", text);
  else if (!strcmp(key, "hatchWavesMessageSoon")) snprintf(T.waves[0], sizeof T.waves[0], "%s", text);
  else if (!strcmp(key, "hatchWavesMessageClose")) snprintf(T.waves[1], sizeof T.waves[1], "%s", text);
  else if (!strcmp(key, "hatchWavesMessageNotClose")) snprintf(T.waves[2], sizeof T.waves[2], "%s", text);
  else if (!strcmp(key, "hatchWavesMessageLongTime")) snprintf(T.waves[3], sizeof T.waves[3], "%s", text);
  else if (!strcmp(key, "gachaTypeLegendary")) snprintf(T.gacha_legendary, sizeof T.gacha_legendary, "%s", text);
  else if (!strcmp(key, "gachaTypeMove")) snprintf(T.gacha_move, sizeof T.gacha_move, "%s", text);
  else if (!strcmp(key, "gachaTypeShiny")) snprintf(T.gacha_shiny, sizeof T.gacha_shiny, "%s", text);
  else if (!strcmp(key, "eventType")) snprintf(T.event_type, sizeof T.event_type, "%s", text);
  else if (!strcmp(key, "sameSpeciesEgg")) snprintf(T.same_species, sizeof T.same_species, "%s", text);
  else CHECK(0, "unknown locale key %s", key);
}

/* the panel for the egg the state shows, as upstream's "name|date" */
static void panel(char* out, size_t n) {
  egm_details d;
  egm_describe(&eggs[L.detail], &T, species_name, NULL, 0, &d);
  snprintf(out, n, "%s|%s", d.name, d.date);
}

/* the grid as the producer prints it: the key of each visible icon ("m" for Manaphy), "-" for a hidden one */
static void grid(char* out, size_t n) {
  size_t o = 0;
  for (int i = 0; i < egl_cell_count(&L) && o + 2 < n; i++) {
    const int e = egl_cell_egg(&L, i);
    if (e < 0) {
      out[o++] = '-';
    } else {
      egm_details d;
      egm_describe(&eggs[e], &T, species_name, NULL, 0, &d);
      out[o++] = !strcmp(d.key, "manaphy") ? 'm' : d.key[0];
    }
  }
  out[o] = 0;
}

static void run_fixture(const char* path) {
  char cmd[512];
  FILE* f;
  const size_t plen = strlen(path);
  if (plen > 3 && !strcmp(path + plen - 3, ".gz")) {
    snprintf(cmd, sizeof cmd, "gzip -dc '%s'", path);
    f = popen(cmd, "r");
  } else {
    f = fopen(path, "r");
  }
  if (!f) {
    printf("FAIL: cannot open %s\n", path);
    failed++;
    return;
  }
  static char line[4096];
  int cases = 0, texts = 0, steps = 0, in_case = 0;
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\n")] = 0;
    if (!strncmp(line, "LOC ", 4)) {
      char key[64];
      const char* rest = line + 4;
      const char* sp = strchr(rest, ' ');
      if (!sp || sp - rest >= (long)sizeof key) continue;
      memcpy(key, rest, (size_t)(sp - rest));
      key[sp - rest] = 0;
      set_text(key, sp + 1);
    } else if (!strncmp(line, "SPN ", 4)) {
      int id;
      char nm[64];
      if (sscanf(line + 4, "%d %63[^\n]", &id, nm) == 2 && id >= 0 && id < 2100) snprintf(names[id], sizeof names[id], "%s", nm);
    } else if (!strncmp(line, "CASE ", 5)) {
      int c, total;
      sscanf(line + 5, "%d %d", &c, &total);
      CHECK(total >= 1 && total <= 200, "total %d", total);
      n_eggs = 0;
      memset(eggs, 0, sizeof eggs);
      in_case = 1;
      cases++;
    } else if (!strncmp(line, "EGG ", 4) && in_case) {
      int i, tier, species, source;
      unsigned long long id, ts;
      int waves;
      if (sscanf(line + 4, "%d %llu %d %d %d %d %llu", &i, &id, &tier, &species, &source, &waves, &ts) != 7) {
        CHECK(0, "bad EGG line %s", line);
        continue;
      }
      CHECK(i == n_eggs, "egg order");
      gc_egg* e = &eggs[n_eggs++];
      e->id = (uint32_t)id;
      e->tier = (uint8_t)tier;
      e->species = (uint16_t)species;
      e->source_type = source < 0 ? 5 : (uint8_t)source; /* a legacy egg has no source: the port holds only 0..4, and 5 has no text */
      e->hatch_waves = waves;
      e->timestamp_ms = ts;
    } else if (!strncmp(line, "TXT ", 4)) {
      int i;
      char key[16];
      sscanf(line + 4, "%d %15[^|]", &i, key);
      const char* p = strchr(line + 4, ' ');
      p = p ? strchr(p + 1, '|') : NULL;
      if (!p) {
        CHECK(0, "bad TXT line");
        continue;
      }
      /* the line is  i key|name|date|waves|info : the key is parsed above, these are the other four */
      char* parts[4];
      char buf[1024];
      snprintf(buf, sizeof buf, "%s", p + 1);
      int np = 0;
      char* q = buf;
      parts[np++] = q;
      while (np < 4 && (q = strchr(q, '|'))) {
        *q++ = 0;
        parts[np++] = q;
      }
      if (np != 4) {
        CHECK(0, "bad TXT fields: %s", line);
        continue;
      }
      const char* info = parts[3];
      egm_details d;
      egm_describe(&eggs[i], &T, species_name, NULL, 0, &d);
      texts++;
      CHECK(!strcmp(d.key, key), "egg %d key: ours %s, upstream %s", i, d.key, key);
      CHECK(!strcmp(d.name, parts[0]), "egg %d name: ours '%s', upstream '%s'", i, d.name, parts[0]);
      CHECK(!strcmp(d.date, parts[1]), "egg %d date: ours '%s', upstream '%s' (ts %llu)", i, d.date, parts[1], (unsigned long long)eggs[i].timestamp_ms);
      CHECK(!strcmp(d.waves, parts[2]), "egg %d waves: ours '%s', upstream '%s'", i, d.waves, parts[2]);
      CHECK(!strcmp(d.type, info), "egg %d type: ours '%s', upstream '%s'", i, d.type, info);
    } else if (!strncmp(line, "NAV0 ", 5)) {
      int cur, scroll, off = 0;
      sscanf(line + 5, "%d %d %n", &cur, &scroll, &off);
      egl_open(&L, n_eggs);
      char ours[200];
      char st[1500];
      panel(st, sizeof st);
      grid(ours, sizeof ours);
      snprintf(st + strlen(st), sizeof st - strlen(st), "|%s", ours);
      CHECK(L.cursor == cur && L.grid.scroll == scroll, "open: cursor %d scroll %d, upstream %d %d", L.cursor, L.grid.scroll, cur, scroll);
      CHECK(!strcmp(st, line + 5 + off), "open: panel/grid ours '%s' upstream '%s'", st, line + 5 + off);
      steps++;
    } else if (!strncmp(line, "NAV ", 4)) {
      int b, ok, cur, scroll, off = 0;
      sscanf(line + 4, "%d %d %d %d %n", &b, &ok, &cur, &scroll, &off);
      const bool r = egl_input(&L, (sg_dir)b);
      char ours[200];
      char st[1500];
      panel(st, sizeof st);
      grid(ours, sizeof ours);
      snprintf(st + strlen(st), sizeof st - strlen(st), "|%s", ours);
      CHECK(r == (ok != 0), "press %d on %d eggs: ours %d upstream %d", b, n_eggs, r, ok);
      CHECK(L.cursor == cur && L.grid.scroll == scroll, "press %d on %d eggs: cursor %d scroll %d, upstream %d %d", b, n_eggs, L.cursor, L.grid.scroll, cur, scroll);
      CHECK(!strcmp(st, line + 4 + off), "press %d on %d eggs: panel/grid ours '%s' upstream '%s'", b, n_eggs, st, line + 4 + off);
      steps++;
    }
  }
  if (plen > 3 && !strcmp(path + plen - 3, ".gz")) pclose(f);
  else fclose(f);
  printf("fixture: %d lists, %d egg texts, %d navigation steps\n", cases, texts, steps);
  CHECK(cases > 0 && texts > 0 && steps > 0, "the fixture is empty");
}

static void units(void) {
  char d[40];
  egm_date(0, 0, d, sizeof d);
  CHECK(!strcmp(d, "Thu, 01/1/1970"), "epoch: %s", d);
  egm_date(1770948693452ull, 0, d, sizeof d);
  CHECK(!strcmp(d, "Fri, 02/13/2026"), "2026-02-13: %s", d);
  egm_date(1709164800000ull, 0, d, sizeof d); /* 2024-02-29 */
  CHECK(!strcmp(d, "Thu, 02/29/2024"), "leap day: %s", d);
  egm_date(1709164800000ull, -1, d, sizeof d); /* a second before: still the 28th */
  CHECK(!strcmp(d, "Wed, 02/28/2024"), "local time before midnight: %s", d);
  egm_date(1709164800000ull, 9 * 3600, d, sizeof d);
  CHECK(!strcmp(d, "Thu, 02/29/2024"), "local time same day: %s", d);
  egm_date(0, -3600, d, sizeof d); /* local time before 1970 */
  CHECK(!strcmp(d, "Wed, 12/31/1969"), "before the epoch: %s", d);
  egm_date(8640000000000001ull, 0, d, sizeof d);
  CHECK(!strcmp(d, "Invalid Date"), "past the range: %s", d);
  egm_date(8640000000000000ull, 0, d, sizeof d);
  CHECK(!strcmp(d, "Sat, 09/13/275760"), "the last day: %s", d);

  gc_egg e;
  memset(&e, 0, sizeof e);
  e.id = 204 * 7;
  CHECK(egm_is_manaphy(&e), "a legacy Common egg with id 1428");
  e.id = 205;
  CHECK(!egm_is_manaphy(&e), "id 205");
  e.id = 204;
  e.tier = 1;
  CHECK(!egm_is_manaphy(&e), "id 204 but Rare");
  e.tier = 0;
  e.species = 25;
  CHECK(!egm_is_manaphy(&e), "id 204 with a species");
  e.id = 1;
  e.species = 489;
  CHECK(egm_is_manaphy(&e), "Phione");
  e.species = 490;
  CHECK(egm_is_manaphy(&e), "Manaphy");
  e.id = 0; /* 0 % 204 == 0: a legacy Common egg with id 0 is a Manaphy egg, as upstream's `!(id % 204)` */
  e.species = 0;
  CHECK(egm_is_manaphy(&e), "id 0");

  /* the grid edges: 1 egg, every press stays on it; 99 eggs (9 rows of 11) never scroll; 100 eggs scroll once */
  gc_egg few[100];
  memset(few, 0, sizeof few);
  for (int n = 1; n <= 100; n += (n == 1 ? 98 : 1)) {
    egl_state s;
    egl_open(&s, n);
    CHECK(s.cursor == 0 && s.grid.scroll == 0 && s.detail == 0, "open on %d", n);
    CHECK(egl_cell_count(&s) == (n < 99 ? n : 99), "cells on %d: %d", n, egl_cell_count(&s));
    /* DOWN through every row and one more: wraps to the top */
    const int rows = (n + EGL_COLS - 1) / EGL_COLS;
    for (int k = 0; k < rows + 1; k++) egl_input(&s, SG_DOWN);
    CHECK(s.detail >= 0 && s.detail < n, "detail inside the list on %d: %d", n, s.detail);
  }
  egl_state s;
  egl_open(&s, 100);
  CHECK(s.grid.scroll == 0, "100 eggs: starts at the top");
  for (int i = 0; i < 9; i++) egl_input(&s, SG_DOWN);
  CHECK(s.grid.scroll == 1, "100 eggs: the tenth row is one scroll away (scroll %d)", s.grid.scroll);
  CHECK(egl_cell_egg(&s, 0) == 11, "100 eggs scrolled: the first cell is egg 11");
  CHECK(egl_cell_egg(&s, 88) == 99, "100 eggs scrolled: cell 88 is egg 99, the last one");
  CHECK(egl_cell_egg(&s, 89) == -1, "100 eggs scrolled: cell 89 is empty");
}

int main(int argc, char** argv) {
  units();
  if (argc > 1) run_fixture(argv[1]);
  else printf("no fixture given: unit checks only\n");
  printf("egglist_model_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
