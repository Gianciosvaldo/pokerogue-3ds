/* What the Game Stats screen shows (see gamestats_model.h). */
#include "gamestats_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"

#define DEX_SHINY 2ull /* DexAttr.SHINY */

static const char* const ENGLISH_ABBR[GSM_ABBR] = {"", "k", "M", "B"};

void gsm_play_time_string(uint32_t s, char* out, int n) {
  /* days padded to two digits, no upper limit */
  const uint32_t days = s / 86400u, hours = (s % 86400u) / 3600u, minutes = (s % 3600u) / 60u, seconds = s % 60u;
  snprintf(out, (size_t)n, "%02u:%02u:%02u:%02u", (unsigned)days, (unsigned)hours, (unsigned)minutes, (unsigned)seconds);
}

/* JavaScript's Number.prototype.toFixed(3) for 0 <= x < 1e6: the exact value of the double, rounded half up (printf rounds half to even) */
static void to_fixed3(double x, char* out, int n) {
  char buf[96];
  snprintf(buf, sizeof buf, "%.60f", x); /* the exact decimal expansion of the double */
  char* dot = strchr(buf, '.');
  if (!dot) {
    snprintf(out, (size_t)n, "%s", buf);
    return;
  }
  const bool up = dot[4] >= '5'; /* the fourth decimal decides: the digits after it only matter when it is 5 and they are 0, still "up" */
  dot[4] = 0;
  /* round the three-decimal string up by one unit in the last place */
  if (up) {
    for (char* p = dot + 3; p >= buf; p--) {
      if (*p == '.') continue;
      if (*p == '9') {
        *p = '0';
        continue;
      }
      (*p)++;
      goto done;
    }
    /* carried out of the first digit: 9.9996 -> 10.000 */
    memmove(buf + 1, buf, strlen(buf) + 1);
    buf[0] = '1';
  }
done:
  snprintf(out, (size_t)n, "%s", buf);
}

void gsm_fancy_number(uint32_t n, const char* const abbr[GSM_ABBR], char* out, int n_out) {
  if (!abbr) abbr = ENGLISH_ABBR;
  /* exponent = floor(log(n) / log(1000)), capped at the table: for a 32-bit n that is the thresholds (the quotient is exact at 10^3, 10^6, 10^9) */
  int e = n >= 1000000000u ? 3 : n >= 1000000u ? 2 : n >= 1000u ? 1 : 0;
  double x = (double)n;
  for (int i = 0; i < e; i++) x /= 1000.0; /* Math.pow(1000, e) as a division (1000^e is exact in a double) */
  if (e == 0 || x == floor(x)) {
    snprintf(out, (size_t)n_out, "%.0f%s", x, abbr[e]);
    return;
  }
  char num[64];
  to_fixed3(x, num, sizeof num);
  snprintf(out, (size_t)n_out, "%s%s", num, abbr[e]);
}

/* "27 (4.7%)": floor(count / total * 1000) / 10, printed as JavaScript prints that number (no trailing ".0") */
static void count_percent(int count, int total, char* out, int n) {
  const int k = total > 0 ? (int)floor((double)count / (double)total * 1000.0) : 0;
  if (k % 10 == 0)
    snprintf(out, (size_t)n, "%d (%d%%)", count, k / 10);
  else
    snprintf(out, (size_t)n, "%d (%d.%d%%)", count, k / 10, k % 10);
}

static int starters_where(const gc_system* sys, bool shiny) {
  int c = 0;
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint64_t a = sys->dex[gc_starter_costs[i].id].caught_attr;
    if (shiny ? (a & DEX_SHINY) != 0 : a != 0) c++;
  }
  return c;
}

/* which dex entries count: 0 = fought (seenCount), 1 = seen (seenAttr or caughtAttr), 2 = caught (caughtAttr) */
static int species_where(const gc_system* sys, int kind) {
  int c = 0;
  for (int i = 0; i < gc_species_dex_info_count; i++) {
    const gc_dex_entry* d = &sys->dex[gc_species_dex_info_data[i].species];
    if (kind == 0 ? d->seen_count != 0 : kind == 1 ? (d->seen_attr != 0 || d->caught_attr != 0) : d->caught_attr != 0) c++;
  }
  return c;
}

void gsm_build(const gc_system* sys, uint32_t play_seconds, const char* const abbr[GSM_ABBR], gsm_row rows[GSM_COUNT]) {
  const gc_game_stats* g = &sys->gstats;
  const gc_stat_milestones* s = &sys->stats;
  memset(rows, 0, sizeof(gsm_row) * GSM_COUNT);
  int n = 0;
#define PLAIN(key, v, hid)                                                       \
  do {                                                                           \
    rows[n].label_key = "gameStatsUiHandler:" key;                               \
    rows[n].hidden = (hid);                                                      \
    snprintf(rows[n].value, sizeof rows[n].value, "%u", (unsigned)(v));          \
    n++;                                                                         \
  } while (0)
#define LABEL(key, hid)                         \
  do {                                          \
    rows[n].label_key = "gameStatsUiHandler:" key; \
    rows[n].hidden = (hid);                     \
  } while (0)
  LABEL("playTime", false);
  gsm_play_time_string(play_seconds, rows[n++].value, sizeof rows[0].value);
  PLAIN("totalBattles", s->total_battles, false);
  LABEL("starters", false);
  count_percent(starters_where(sys, false), gc_starter_cost_count, rows[n++].value, sizeof rows[0].value);
  LABEL("shinyStarters", false);
  count_percent(starters_where(sys, true), gc_starter_cost_count, rows[n++].value, sizeof rows[0].value);
  LABEL("speciesEncountered", false);
  count_percent(species_where(sys, 0), gc_species_dex_info_count, rows[n++].value, sizeof rows[0].value);
  LABEL("speciesSeen", false);
  count_percent(species_where(sys, 1), gc_species_dex_info_count, rows[n++].value, sizeof rows[0].value);
  LABEL("speciesCaught", false);
  count_percent(species_where(sys, 2), gc_species_dex_info_count, rows[n++].value, sizeof rows[0].value);
  PLAIN("ribbonsOwned", s->ribbons_owned, false);
  PLAIN("classicRuns", g->classic_sessions_played, false);
  PLAIN("classicWins", s->classic_wins, false);
  PLAIN("dailyRunAttempts", g->daily_run_sessions_played, false);
  PLAIN("dailyRunWins", g->daily_run_sessions_won, false);
  PLAIN("endlessRuns", g->endless_sessions_played, true);
  PLAIN("highestWaveEndless", g->highest_endless_wave, true);
  LABEL("highestMoney", false);
  gsm_fancy_number(g->highest_money, abbr, rows[n++].value, sizeof rows[0].value);
  PLAIN("highestDamage", s->highest_damage, false);
  PLAIN("highestHpHealed", g->highest_heal, false);
  PLAIN("pokemonEncountered", g->pokemon_seen, false);
  PLAIN("pokemonDefeated", g->pokemon_defeated, false);
  PLAIN("pokemonCaught", s->total_catches, false);
  PLAIN("eggsHatched", g->pokemon_hatched, false);
  PLAIN("subLegendsSeen", g->sub_legendary_seen, true);
  PLAIN("subLegendsCaught", g->sub_legendary_caught, true);
  PLAIN("subLegendsHatched", g->sub_legendary_hatched, true);
  PLAIN("legendsSeen", g->legendary_seen, true);
  PLAIN("legendsCaught", g->legendary_caught, true);
  PLAIN("legendsHatched", g->legendary_hatched, true);
  PLAIN("mythicalsSeen", g->mythical_seen, true);
  PLAIN("mythicalsCaught", g->mythical_caught, true);
  PLAIN("mythicalsHatched", g->mythical_hatched, true);
  PLAIN("shiniesSeen", g->shiny_seen, true);
  PLAIN("shiniesCaught", g->shiny_caught, true);
  PLAIN("shiniesHatched", g->shiny_hatched, true);
  PLAIN("pokemonFused", g->pokemon_fused, true);
  PLAIN("trainersDefeated", g->trainers_defeated, false);
  PLAIN("eggsPulled", s->eggs_pulled, true);
  PLAIN("rareEggsPulled", s->rare_eggs_pulled, true);
  PLAIN("epicEggsPulled", s->epic_eggs_pulled, true);
  PLAIN("legendaryEggsPulled", s->legendary_eggs_pulled, true);
  PLAIN("manaphyEggsPulled", s->manaphy_eggs_pulled, true);
#undef PLAIN
#undef LABEL
}

bool gsm_label_hidden(const gsm_row* r) {
  /* upstream: `!hidden || Number.isNaN(value) || parseInt(value)`: a hidden stat is "???" while parseInt gives 0 (or NaN, which is falsy) */
  return r->hidden && strtol(r->value, NULL, 10) == 0;
}
