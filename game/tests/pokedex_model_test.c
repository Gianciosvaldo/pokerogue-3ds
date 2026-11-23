/* pokedex_model_test: the Pokedex list's model (source/pokedex_model.c) against upstream's own PokedexUiHandler.
 *
 *   make pokedex_model_test && build/pokedex_model_test fixtures/pokedex-list.native.txt.gz
 *
 * The fixture is made by pokedex-list.producer.test.ts in the pinned upstream checkout (Node 22): the real handler, headless, over random dex data
 * (every species' seen and caught attributes, with some stored form preferences), opened with its default filters and pressed with random UP / DOWN /
 * LEFT / RIGHT (leaning DOWN, then UP, to reach the scrolled rows and the end of the list). After every press this compares the result, the cursor, the
 * scroll position, the mode upstream went to (the filter bar or the filter text: the handler is put back in the grid), the info panel (number, name,
 * the two type icons), the species of the first cell and the tint of every visible icon. Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"
#include "gc/run.h"
#include "pokedex_model.h"

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

static gc_system sys;
static pdx_state P;
static char names[9100][64];
static char number_fmt[64] = "No. {{dexNo}}";
static const char* const TYPE_FRAME[20] = {"normal", "fighting", "flying",   "poison", "ground", "rock",  "bug",  "ghost",  "steel", "fire",
                                           "water",  "grass",    "electric", "psychic", "ice",   "dragon", "dark", "fairy", "stellar", "unknown"};

/* the producer's state string: number|name|type1|type2|first species|tints */
static void state_string(char* out, size_t n) {
  pdx_panel p;
  pdx_panel_of(&P, &p);
  char number[32];
  const char* at = strstr(number_fmt, "{{dexNo}}");
  if (at) snprintf(number, sizeof number, "%.*s%04d%s", (int)(at - number_fmt), number_fmt, p.dex_number, at + 9);
  else snprintf(number, sizeof number, "%04d", p.dex_number);
  char grid[PDX_CELLS + 1];
  for (int i = 0; i < PDX_CELLS; i++) grid[i] = pdx_cell_visible(&P, i) ? pdx_cell_tint(&P, i) : '-';
  grid[PDX_CELLS] = 0;
  snprintf(out, n, "%s|%s|%s|%s|%u|%s", number, p.name_shown ? names[p.species] : "???", p.type1 < 0 ? "-" : TYPE_FRAME[p.type1],
           p.type2 < 0 ? "-" : TYPE_FRAME[p.type2], (unsigned)P.cell_species[0], grid);
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
  int cases = 0, steps = 0, starters_seen = 0, modes[3] = {0, 0, 0};
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\n")] = 0;
    if (!strncmp(line, "LOC pokemonNumber ", 18)) {
      snprintf(number_fmt, sizeof number_fmt, "%.63s", line + 18);
    } else if (!strncmp(line, "SPN ", 4)) {
      int id;
      char nm[64];
      if (sscanf(line + 4, "%d %63[^\n]", &id, nm) == 2 && id >= 0 && id < 9100) snprintf(names[id], sizeof names[id], "%s", nm);
    } else if (!strncmp(line, "STARTER ", 8)) {
      int id, st, is;
      if (sscanf(line + 8, "%d %d %d", &id, &st, &is) == 3) {
        starters_seen++;
        CHECK(pdx_starter_of((uint16_t)id) == st, "getStarter(%d): ours %d, upstream %d", id, pdx_starter_of((uint16_t)id), st);
        CHECK((gc_starter_cost(id) >= 0) == (is != 0), "isStarter(%d): upstream %d", id, is);
      }
    } else if (!strncmp(line, "CASE ", 5)) {
      memset(&sys, 0, sizeof sys);
      cases++;
    } else if (!strncmp(line, "DEX ", 4)) {
      int id;
      unsigned long long seen, caught;
      if (sscanf(line + 4, "%d %llu %llu", &id, &seen, &caught) == 3 && id > 0 && id <= GC_MAX_SPECIES_ID) {
        sys.dex[id].seen_attr = seen;
        sys.dex[id].caught_attr = caught;
      }
    } else if (!strncmp(line, "PREF ", 5)) {
      int id, form;
      if (sscanf(line + 5, "%d %d", &id, &form) == 2) {
        sys.starters[id].preference_flags |= 8;
        sys.starters[id].form_preference = (uint8_t)form;
      }
    } else if (!strncmp(line, "OPEN ", 5)) {
      int n, cur, scroll, off = 0;
      sscanf(line + 5, "%d %d %d %n", &n, &cur, &scroll, &off);
      pdx_open(&P, &sys);
      char ours[1024];
      state_string(ours, sizeof ours);
      CHECK(P.n == n, "list of %d species, upstream %d", P.n, n);
      CHECK(P.cursor == cur && P.scroll == scroll, "open: cursor %d scroll %d, upstream %d %d", P.cursor, P.scroll, cur, scroll);
      CHECK(!strcmp(ours, line + 5 + off), "open: ours '%s' upstream '%s'", ours, line + 5 + off);
    } else if (!strncmp(line, "NAV ", 4)) {
      int b, ok, cur, scroll, off = 0;
      char mode[8];
      sscanf(line + 4, "%d %d %d %d %7s %n", &b, &ok, &cur, &scroll, mode, &off);
      pdx_mode m;
      const bool r = pdx_input(&P, (pdx_button)b, &m);
      const int want = !strcmp(mode, "bar") ? PDX_FILTER_BAR : !strcmp(mode, "text") ? PDX_FILTER_TEXT : PDX_GRID;
      modes[m]++;
      char ours[1024];
      state_string(ours, sizeof ours);
      CHECK(r == (ok != 0), "press %d: ours %d upstream %d", b, r, ok);
      CHECK((int)m == want, "press %d: mode %d, upstream %s", b, (int)m, mode);
      CHECK(P.cursor == cur && P.scroll == scroll, "press %d: cursor %d scroll %d, upstream %d %d", b, P.cursor, P.scroll, cur, scroll);
      CHECK(!strcmp(ours, line + 4 + off), "press %d at %d/%d: ours '%s' upstream '%s'", b, P.cursor, P.scroll, ours, line + 4 + off);
      steps++;
    }
  }
  if (plen > 3 && !strcmp(path + plen - 3, ".gz")) pclose(f);
  else fclose(f);
  printf("fixture: %d cases, %d presses (%d in the grid, %d into the filter bar, %d into the filter text)\n", cases, steps, modes[0], modes[1], modes[2]);
  CHECK(cases > 0 && steps > 0 && starters_seen == 1084, "the fixture is empty or incomplete (%d starters)", starters_seen);
}

static void units(void) {
  memset(&sys, 0, sizeof sys);
  pdx_open(&P, &sys);
  CHECK(P.n == 1084 && P.n == gc_species_dex_info_count, "the list holds the %d species of the dex", P.n);
  bool ascending = true;
  for (int i = 1; i < P.n; i++)
    if (P.ids[i] <= P.ids[i - 1]) ascending = false;
  CHECK(ascending && P.ids[0] == 1, "ascending from Bulbasaur");
  CHECK(P.cursor == 0 && P.scroll == 0, "opens at the top");
  /* nothing seen or caught: every icon black, the panel hides everything */
  CHECK(pdx_cell_tint(&P, 0) == 'b', "an unseen species is black");
  pdx_panel p;
  pdx_panel_of(&P, &p);
  CHECK(p.species == 1 && p.dex_number == 1 && !p.name_shown && p.type1 < 0 && p.type2 < 0, "the panel of an unseen species");
  /* seen: grey, name, types */
  sys.dex[1].seen_attr = 1;
  CHECK(pdx_cell_tint(&P, 0) == 'g' && pdx_is_seen(&sys, 1), "a seen species is grey");
  pdx_panel_of(&P, &p);
  CHECK(p.name_shown && p.type1 == 11 && p.type2 == 3, "seen Bulbasaur shows Grass / Poison");
  /* an evolution of a caught starter counts as seen though its own entry is empty: its types show but its name is hidden */
  memset(&sys, 0, sizeof sys);
  sys.dex[1].caught_attr = 1 | (1u << 7);
  pdx_open(&P, &sys);
  P.cursor = 1; /* Ivysaur */
  pdx_panel_of(&P, &p);
  CHECK(p.species == 2 && !p.name_shown && p.type1 == 11 && p.type2 == 3, "Ivysaur of a caught Bulbasaur: types, no name");
  CHECK(pdx_cell_tint(&P, 1) == 'g', "and a grey icon");
  /* a stored form preference picks the form the panel shows: a species with two selectable forms of other types, both caught */
  {
    int sp = 0, f = 0;
    for (int i = 0; i < gc_species_dex_info_count && !sp; i++) {
      const uint16_t id = gc_species_dex_info_data[i].species;
      const uint64_t sf = gc_species_dex_info_data[i].starter_forms;
      const uint64_t full = gc_species_dex_info_data[i].full_unlocks;
      if (gc_starter_cost(id) < 0) continue;
      for (int k = 1; k < 8 && !sp; k++) {
        if (!(sf >> k & 1u) || !(full >> (7 + k) & 1u) || !(full >> 7 & 1u)) continue;
        pdx_panel a, b;
        memset(&sys, 0, sizeof sys);
        sys.dex[id].caught_attr = (1ull << 7) | (1ull << (7 + k)) | 1;
        pdx_open(&P, &sys);
        for (int c = 0; c < PDX_CELLS; c++) P.cell_species[c] = id;
        pdx_panel_of(&P, &a); /* the first caught form: form 0 */
        sys.starters[id].preference_flags |= 8;
        sys.starters[id].form_preference = (uint8_t)k;
        pdx_panel_of(&P, &b);
        if (a.type1 != b.type1 || a.type2 != b.type2) {
          sp = id;
          f = k;
        }
      }
    }
    CHECK(sp != 0, "a species whose selectable forms differ in type exists");
    if (sp) {
      pdx_panel a;
      pdx_panel_of(&P, &a);
      CHECK(a.type1 >= 0, "species %d form %d: preferred form shown", sp, f);
      sys.starters[sp].preference_flags = 0;
      pdx_panel b;
      pdx_panel_of(&P, &b);
      CHECK(a.type1 != b.type1 || a.type2 != b.type2, "without the preference the first caught form shows (species %d)", sp);
      sys.starters[sp].preference_flags = 8;
      sys.starters[sp].form_preference = 0; /* a stored form 0 is not a preference (upstream's `if (formIndex)`) */
      pdx_panel c;
      pdx_panel_of(&P, &c);
      CHECK(c.type1 == b.type1 && c.type2 == b.type2, "a stored form 0 falls back to the first caught form");
      sys.starters[sp].form_preference = (uint8_t)f;
      sys.dex[sp].caught_attr &= ~(1ull << (7 + f)); /* the preferred form is no longer caught: the preference is dropped */
      sys.dex[sp].caught_attr |= (1ull << 7);
      pdx_panel d;
      pdx_panel_of(&P, &d);
      CHECK(d.type1 == b.type1 && d.type2 == b.type2, "a preference for a form that is not caught is ignored");
    }
  }
  /* the end of the list: DOWN wraps to the top row, RIGHT at the end of the last row asks for the filter text */
  pdx_mode m;
  for (int k = 0; k < 200; k++) pdx_input(&P, PDX_DOWN, &m);
  CHECK(P.scroll >= 0 && P.cursor >= 0 && P.cursor < PDX_CELLS, "inside the grid after 200 presses down");
  pdx_open(&P, &sys);
  CHECK(!pdx_input(&P, PDX_UP, &m) || m == PDX_FILTER_BAR, "UP from the first row enters the filter bar");
  CHECK(m == PDX_FILTER_BAR && P.cursor == 0 && P.scroll == 0, "and changes nothing else");
  pdx_input(&P, PDX_LEFT, &m);
  CHECK(m == PDX_FILTER_TEXT, "LEFT from the first column enters the filter text");
}

int main(int argc, char** argv) {
  units();
  if (argc > 1) run_fixture(argv[1]);
  else printf("no fixture given: unit checks only\n");
  printf("pokedex_model_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
