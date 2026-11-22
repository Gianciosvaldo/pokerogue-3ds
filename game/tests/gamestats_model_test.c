/* gamestats_model_test: what the Game Stats screen shows (source/gamestats_model.c) against upstream's own GameStatsUiHandler.
 *
 *   make gamestats_model_test && build/gamestats_model_test fixtures/game-stats-screen.native.txt.gz
 *
 * The fixture is made by gamestats-screen.producer.test.ts in the pinned upstream checkout (Node 22): the real handler, headless, over random
 * profiles (35 counters and some dex entries), read at every scroll position. Per case this builds the same gc_system, runs gsm_build and compares
 * the 40 rows: the value text, and the label ("???" for a hidden stat that is 0, else upstream's English text for the key). Also the page
 * geometry (18 slots, 2 columns, last position 11) and unit checks of the number formats (play time, abbreviated money, percentages).
 * Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gamestats_model.h"
#include "gc/data.h"

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
static gsm_row rows[GSM_COUNT];

typedef struct {
  const char* name;
  uint32_t* where;
} gs_field;

static gs_field fields[64];
static int n_fields;
static void reg(const char* n, uint32_t* p) {
  fields[n_fields].name = n;
  fields[n_fields++].where = p;
}
static void register_fields(void) {
  n_fields = 0;
  reg("playTime", NULL); /* passed in, not stored */
  reg("battles", &sys.stats.total_battles);
  reg("ribbonsOwned", &sys.stats.ribbons_owned);
  reg("classicSessionsPlayed", &sys.gstats.classic_sessions_played);
  reg("sessionsWon", &sys.stats.classic_wins);
  reg("dailyRunSessionsPlayed", &sys.gstats.daily_run_sessions_played);
  reg("dailyRunSessionsWon", &sys.gstats.daily_run_sessions_won);
  reg("endlessSessionsPlayed", &sys.gstats.endless_sessions_played);
  reg("highestEndlessWave", &sys.gstats.highest_endless_wave);
  reg("highestMoney", &sys.gstats.highest_money);
  reg("highestDamage", &sys.stats.highest_damage);
  reg("highestHeal", &sys.gstats.highest_heal);
  reg("pokemonSeen", &sys.gstats.pokemon_seen);
  reg("pokemonDefeated", &sys.gstats.pokemon_defeated);
  reg("pokemonCaught", &sys.stats.total_catches);
  reg("pokemonHatched", &sys.gstats.pokemon_hatched);
  reg("subLegendaryPokemonSeen", &sys.gstats.sub_legendary_seen);
  reg("subLegendaryPokemonCaught", &sys.gstats.sub_legendary_caught);
  reg("subLegendaryPokemonHatched", &sys.gstats.sub_legendary_hatched);
  reg("legendaryPokemonSeen", &sys.gstats.legendary_seen);
  reg("legendaryPokemonCaught", &sys.gstats.legendary_caught);
  reg("legendaryPokemonHatched", &sys.gstats.legendary_hatched);
  reg("mythicalPokemonSeen", &sys.gstats.mythical_seen);
  reg("mythicalPokemonCaught", &sys.gstats.mythical_caught);
  reg("mythicalPokemonHatched", &sys.gstats.mythical_hatched);
  reg("shinyPokemonSeen", &sys.gstats.shiny_seen);
  reg("shinyPokemonCaught", &sys.gstats.shiny_caught);
  reg("shinyPokemonHatched", &sys.gstats.shiny_hatched);
  reg("pokemonFused", &sys.gstats.pokemon_fused);
  reg("trainersDefeated", &sys.gstats.trainers_defeated);
  reg("eggsPulled", &sys.stats.eggs_pulled);
  reg("rareEggsPulled", &sys.stats.rare_eggs_pulled);
  reg("epicEggsPulled", &sys.stats.epic_eggs_pulled);
  reg("legendaryEggsPulled", &sys.stats.legendary_eggs_pulled);
  reg("manaphyEggsPulled", &sys.stats.manaphy_eggs_pulled);
}

#define MAX_LABELS 64
static char label_key[MAX_LABELS][64], label_text[MAX_LABELS][96];
static int n_labels;
static const char* english(const char* key) {
  const char* k = strchr(key, ':');
  k = k ? k + 1 : key;
  for (int i = 0; i < n_labels; i++)
    if (!strcmp(label_key[i], k)) return label_text[i];
  return NULL;
}

static const char* abbr_tbl[GSM_ABBR] = {"", "k", "M", "B"};
static char abbr_store[3][8];

static uint32_t play_seconds;
static void apply_gs(char* line) {
  for (char* tok = strtok(line, " "); tok; tok = strtok(NULL, " ")) {
    char* eq = strchr(tok, '=');
    if (!eq) continue;
    *eq = 0;
    const unsigned long long v = strtoull(eq + 1, NULL, 10);
    int found = 0;
    for (int i = 0; i < n_fields; i++)
      if (!strcmp(fields[i].name, tok)) {
        if (fields[i].where)
          *fields[i].where = (uint32_t)v;
        else
          play_seconds = (uint32_t)v;
        found = 1;
      }
    CHECK(found, "unknown counter %s", tok);
    CHECK(v <= 0xFFFFFFFFull, "%s = %llu does not fit 32 bits", tok, v);
  }
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
  static char line[2048];
  int cases = 0, rows_seen = 0, in_case = 0;
  gc_system fresh;
  gc_system_init_defaults(&fresh);
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "LABEL ", 6)) {
      char* key = line + 6;
      char* sp = strchr(key, ' ');
      CHECK(sp != NULL, "bad label line");
      if (!sp || n_labels >= MAX_LABELS) continue;
      *sp = 0;
      snprintf(label_key[n_labels], sizeof label_key[0], "%s", key);
      snprintf(label_text[n_labels], sizeof label_text[0], "%s", sp + 1);
      n_labels++;
    } else if (!strncmp(line, "ABBR ", 5)) {
      char a[8], b[8], c[8];
      CHECK(sscanf(line + 5, "%7s %7s %7s", a, b, c) == 3, "bad abbr line");
      snprintf(abbr_store[0], 8, "%s", a);
      snprintf(abbr_store[1], 8, "%s", b);
      snprintf(abbr_store[2], 8, "%s", c);
      abbr_tbl[1] = abbr_store[0];
      abbr_tbl[2] = abbr_store[1];
      abbr_tbl[3] = abbr_store[2];
    } else if (!strncmp(line, "SPECIES ", 8)) {
      int species = 0, starters = 0;
      CHECK(sscanf(line, "SPECIES %d STARTERS %d", &species, &starters) == 2, "bad species line");
      CHECK(species == gc_species_dex_info_count, "upstream has %d species, game-core's dex %d", species, gc_species_dex_info_count);
      CHECK(starters == gc_starter_cost_count, "upstream has %d starters, game-core's table %d", starters, gc_starter_cost_count);
    } else if (!strncmp(line, "CASE ", 5)) {
      sys = fresh; /* the profile a new game has */
      play_seconds = 0;
      in_case = 1;
      cases++;
    } else if (!strncmp(line, "GS ", 3)) {
      apply_gs(line + 3);
    } else if (!strncmp(line, "DEX ", 4)) {
      unsigned id;
      unsigned long long seen_count, seen_attr, caught_attr;
      CHECK(sscanf(line + 4, "%u %llu %llu %llu", &id, &seen_count, &seen_attr, &caught_attr) == 4, "bad dex line");
      CHECK(id <= GC_MAX_SPECIES_ID, "species %u", id);
      if (id <= GC_MAX_SPECIES_ID) {
        sys.dex[id].seen_count = (uint32_t)seen_count;
        sys.dex[id].seen_attr = seen_attr;
        sys.dex[id].caught_attr = caught_attr;
      }
    } else if (!strncmp(line, "PAGE ", 5)) {
      int per = 0, cols = 0, maxc = 0;
      CHECK(sscanf(line, "PAGE perPage=%d columns=%d maxCursor=%d", &per, &cols, &maxc) == 3, "bad page line");
      CHECK(per == GSM_PER_PAGE && cols == GSM_COLUMNS && maxc == GSM_MAX_CURSOR, "page %d x %d max %d, here %d x %d max %d", per, cols, maxc,
            GSM_PER_PAGE, GSM_COLUMNS, GSM_MAX_CURSOR);
      gsm_build(&sys, play_seconds, abbr_tbl, rows);
    } else if (!strncmp(line, "ROW ", 4)) {
      int idx = -1, off = 0;
      CHECK(sscanf(line + 4, "%d %n", &idx, &off) >= 1 && idx >= 0 && idx < GSM_COUNT, "bad row line");
      if (idx < 0 || idx >= GSM_COUNT) continue;
      char* text = line + 4 + off;
      char* bar = strrchr(text, '|');
      CHECK(bar != NULL, "row without a value: %s", text);
      if (!bar) continue;
      *bar = 0;
      const char* want_value = bar + 1;
      const char* want_label = text;
      const char* shown = gsm_label_hidden(&rows[idx]) ? "???" : english(rows[idx].label_key);
      CHECK(shown && !strcmp(shown, want_label), "case %d row %d label: upstream '%s', here '%s'", cases - 1, idx, want_label, shown ? shown : "(none)");
      CHECK(!strcmp(rows[idx].value, want_value), "case %d row %d (%s) value: upstream '%s', here '%s'", cases - 1, idx, rows[idx].label_key,
            want_value, rows[idx].value);
      rows_seen++;
    } else if (!strcmp(line, "END")) {
      in_case = 0;
    }
  }
  CHECK(!in_case, "the trace ended inside a case");
  CHECK(cases > 0 && rows_seen == cases * GSM_COUNT, "%d cases, %d rows", cases, rows_seen);
  if (plen > 3 && !strcmp(path + plen - 3, ".gz"))
    pclose(f);
  else
    fclose(f);
  printf("fixture: %d cases, %d rows compared\n", cases, rows_seen);
}

static void units(void) {
  char b[64];
  gsm_play_time_string(0, b, sizeof b);
  CHECK(!strcmp(b, "00:00:00:00"), "%s", b);
  gsm_play_time_string(123456, b, sizeof b);
  CHECK(!strcmp(b, "01:10:17:36"), "%s", b);
  gsm_play_time_string(86400u * 123 + 3661, b, sizeof b);
  CHECK(!strcmp(b, "123:01:01:01"), "%s", b); /* days are not capped at two digits */
  gsm_fancy_number(999, NULL, b, sizeof b);
  CHECK(!strcmp(b, "999"), "%s", b);
  gsm_fancy_number(1000, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1k"), "%s", b);
  gsm_fancy_number(1500, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1.500k"), "%s", b);
  gsm_fancy_number(999999, NULL, b, sizeof b);
  CHECK(!strcmp(b, "999.999k"), "%s", b);
  gsm_fancy_number(1000000, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1M"), "%s (the quotient is exact at a power of 1000)", b);
  gsm_fancy_number(1000000000u, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1B"), "%s", b);
  gsm_fancy_number(1062500, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1.063M"), "%s (toFixed rounds an exact tie up)", b);
  gsm_fancy_number(4294967295u, NULL, b, sizeof b);
  CHECK(!strcmp(b, "4.295B"), "%s", b);
  gsm_fancy_number(999999999u, NULL, b, sizeof b);
  CHECK(!strcmp(b, "1000.000M"), "%s (upstream: 999999999 / 1e6 = 999.999999, toFixed(3) = 1000.000)", b);
  /* a hidden stat reads "???" while it is 0 and its own label from 1 */
  gsm_row r = {"gameStatsUiHandler:endlessRuns", true, "0"};
  CHECK(gsm_label_hidden(&r), "hidden at 0");
  snprintf(r.value, sizeof r.value, "3");
  CHECK(!gsm_label_hidden(&r), "shown at 3");
  r.hidden = false;
  snprintf(r.value, sizeof r.value, "0");
  CHECK(!gsm_label_hidden(&r), "a plain stat shows its label at 0");
  CHECK(GSM_MAX_CURSOR == 11, "last position %d", GSM_MAX_CURSOR);
  /* a new profile: the 27 default starters of 572 */
  gc_system_init_defaults(&sys);
  gsm_build(&sys, 0, NULL, rows);
  CHECK(!strcmp(rows[2].value, "27 (4.7%)"), "%s", rows[2].value);
  CHECK(!strcmp(rows[3].value, "0 (0%)"), "%s", rows[3].value);
}

int main(int argc, char** argv) {
  register_fields();
  units();
  if (argc > 1)
    run_fixture(argv[1]);
  else
    printf("no fixture given: unit checks only\n");
  printf("gamestats_model_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
