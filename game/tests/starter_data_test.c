/* starter_data_test: what the starter select screen gets from the game (source/starter_data.c) and the system data on the SD
 * card (source/system_data.c), over real game-core and the real save.c, on the host.
 *
 *   build/starter_data_test [random systems, default 300]
 *
 * Checks, in a temporary folder (the SD card's "sdmc:/..." paths are relative here):
 *   - system data: a new install starts with the defaults and writes them at the first save; a save of unchanged data writes
 *     nothing; changes survive a restart; a file that is damaged or from a newer version is never written over, never deleted,
 *     and the game goes on with the defaults;
 *   - the list, the panel and the team of a new save and of a save with everything unlocked, against game-core's own functions;
 *   - the team the screen hands over is accepted by gc_run_set_starters, for the new save and for random systems built the way a run
 *     builds them (catches of random Pokemon through gc_system_set_pokemon_caught), for every listed species alone and for random
 *     teams within the budget. A species whose defaults game-core refuses would be one the player sees and cannot start with.
 * Exit 1 on a failed check. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
static time_t starter_clock=1792627200;
time_t time(time_t *out){if(out)*out=starter_clock;return starter_clock;}


#include "gc/data.h"
#include "gc/run.h"
static bool rotating_carrier(uint16_t species){uint16_t ids[3];gc_pokerus_starters((uint64_t)starter_clock*1000,ids);return species==ids[0]||species==ids[1]||species==ids[2];}
#include "pr_data.h"
#include "pr_enums.h"
#include "save.h"
#include "starter_data.h"
#include "starter_filter.h"
#include "system_data.h"

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

const pr_species* menu_species(uint16_t id);
const char* i18n_species_name(int id) {
  const pr_species* sp = menu_species((uint16_t)id);
  return sp ? sp->name : "?";
}

/* ------------------------------------------------------------------ helpers */

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;
static uint32_t rnd(uint32_t n) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return n ? (uint32_t)((rng_state >> 11) % n) : 0;
}

static gc_system S1, S2; /* 724 KB each: not on the stack */

static bool exists(const char* p) { return access(p, F_OK) == 0; }
static size_t file_size(const char* p) {
  struct stat st;
  return stat(p, &st) == 0 ? (size_t)st.st_size : 0;
}
static void write_raw(const char* path, const void* data, size_t n) {
  FILE* f = fopen(path, "wb");
  if (f) {
    fwrite(data, 1, n, f);
    fclose(f);
  }
}
static bool same_file(const char* p, const void* data, size_t n) {
  FILE* f = fopen(p, "rb");
  if (!f) return false;
  uint8_t* b = malloc(n + 1);
  const size_t got = fread(b, 1, n + 1, f);
  fclose(f);
  const bool same = got == n && !memcmp(b, data, n);
  free(b);
  return same;
}

/* a caught Pokemon, as a run makes it */
static gc_mon make_mon(uint16_t species, int form, bool shiny, int variant, int ability_index, int nature) {
  gc_mon m;
  memset(&m, 0, sizeof m);
  const pr_species* sp = menu_species(species);
  m.b.species = species;
  m.b.form = (uint8_t)form;
  m.b.gender = sp && sp->male_permille < 0 ? -1 : (int8_t)(rnd(2));
  m.b.shiny = shiny;
  m.variant = (uint8_t)variant;
  m.ability_index = (uint8_t)ability_index;
  m.b.nature = (uint8_t)nature;
  for (int i = 0; i < 6; i++) m.b.ivs[i] = (uint8_t)rnd(32);
  m.b.level = 20;
  return m;
}

static bool has_variants(uint16_t species) {
  for (int i = 0; i < gc_species_dex_info_count; i++)
    if (gc_species_dex_info_data[i].species == species) return gc_species_dex_info_data[i].has_variants;
  return false;
}

/* everything a species can unlock, unlocked */
static void unlock_all(gc_system* sys) {
  gc_system_init_defaults(sys);
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    sys->dex[id].caught_attr = gc_species_full_unlocks_data(id) | 1ull;
    sys->dex[id].seen_attr = sys->dex[id].caught_attr;
    sys->dex[id].nature_attr = 0x3FFFFFEu;
    sys->starters[id].ability_attr = 7;
    sys->starters[id].passive_attr = 3;
    sys->starters[id].value_reduction = (uint32_t)rnd(3);
    for (int k = 0; k < 6; k++) sys->dex[id].ivs[k] = (uint8_t)rnd(32);
  }
}

/* the system after a run caught `n` random Pokemon (a species, a form, a shininess, an ability, a nature), through game-core */
static void random_system(gc_system* sys, int n) {
  gc_system_init_defaults(sys);
  for (int i = 0; i < n; i++) {
    const pr_species* sp = &pr_species_table[rnd(PR_SPECIES_COUNT)];
    int form = 0;
    if (sp->nforms > 1 && rnd(4) == 0) form = (int)rnd((uint32_t)sp->nforms);
    const pr_form* f = &pr_forms[sp->form0 + form];
    if (f->unobtainable) form = 0; /* a catch is never of a battle-only form (a Mega is caught as its base form) */
    f = &pr_forms[sp->form0 + form];
    const bool shiny = rnd(3) == 0;
    /* only a species with variants has a shiny of variant 2 or 3 */
    const gc_mon m = make_mon((uint16_t)sp->id, form, shiny, has_variants((uint16_t)sp->id) ? (int)rnd(3) : 0,
                              f->ability_hidden && rnd(3) == 0 ? 2 : (int)rnd(2), (int)rnd(25));
    gc_system_set_pokemon_caught(sys, &m, true, false);
    gc_system_update_species_dex_ivs(sys, (uint16_t)sp->id, m.b.ivs);
  }
  /* the unlocks that candy buys */
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    if (!sys->dex[id].caught_attr) continue;
    if (rnd(3) == 0) sys->starters[id].value_reduction = rnd(3);
    if (rnd(4) == 0) sys->starters[id].passive_attr = 1 + rnd(3);
    if (rnd(6) == 0) sys->starters[id].classic_win_count = 1;
  }
}

/* a run that takes the team: game-core's own verdict on it */
static gc_run RUN;
static bool core_takes(const gc_system* sys, const gc_starter* team, int n, const char** why) {
  gc_run_new(&RUN, GAME_MODES_CLASSIC, "starterdatatestseed00001", 7);
  RUN.sys = (gc_system*)sys;
  return gc_run_set_starters(&RUN, team, n, why);
}

/* ------------------------------------------------------------------ system data on the SD card */

static void test_system_data(void) {
  char p[256], why[SYSD_WHY_LEN];
  snprintf(p, sizeof p, "%s/system.sav", SAVE_DIR);

  /* a new install */
  sysd_reset();
  CHECK(sysd_state(why) == SYSD_NEW && !why[0], "new install: state %d", sysd_state(NULL));
  gc_system* sys = sysd_get();
  CHECK(sys->dex[gc_default_starters[0]].caught_attr == 157, "the defaults: %llu", (unsigned long long)sys->dex[gc_default_starters[0]].caught_attr);
  CHECK(!exists(p), "nothing is written before the first save");
  CHECK(sysd_save() && exists(p), "the first save writes the defaults");
  const size_t first_size = file_size(p);
  CHECK(first_size > 0 && first_size < 16 * 1024, "the defaults are small: %zu bytes", first_size);

  /* unchanged: nothing is written */
  remove(p);
  CHECK(sysd_save() && !exists(p), "an unchanged system is not written again");
  sys->dex[1].seen_count++;
  CHECK(sysd_save() && exists(p), "a change is written");

  /* changes survive a restart; the file is the whole system */
  const gc_mon m = make_mon(25, 0, true, 2, 1, 7);
  gc_system_set_pokemon_caught(sys, &m, true, false);
  CHECK(sysd_save(), "save after a catch");
  memcpy(&S1, sys, sizeof S1);
  sysd_reset();
  CHECK(sysd_state(why) == SYSD_LOADED, "restart: state %d (%s)", sysd_state(NULL), why);
  CHECK(!memcmp(&S1, sysd_get(), sizeof S1), "the system read back is the one written");
  remove(p);
  CHECK(sysd_save() && !exists(p), "a system that was read is not written back while unchanged");

  /* a damaged file */
  sys = sysd_get();
  CHECK(sysd_save() && !exists(p), "still unchanged");
  uint8_t junk[300];
  memset(junk, 0x5a, sizeof junk);
  write_raw(p, junk, sizeof junk);
  char bak[300];
  snprintf(bak, sizeof bak, "%s.bak", p);
  remove(bak);
  sysd_reset();
  CHECK(sysd_state(why) == SYSD_UNREADABLE && why[0], "damaged: state %d", sysd_state(NULL));
  printf("  damaged file says: \"%s\"\n", why);
  sys = sysd_get();
  CHECK(sys->dex[gc_default_starters[0]].caught_attr == 157 && sys->dex[m.b.species].caught_attr == 0, "damaged: the game goes on with the defaults");
  sys->dex[1].seen_count = 5;
  CHECK(sysd_save(), "damaged: nothing to write, and no failure to report");
  CHECK(same_file(p, junk, sizeof junk), "damaged: the file is untouched");
  CHECK(sysd_state(NULL) == SYSD_UNREADABLE, "damaged: still reported");

  /* a file from a newer version: the same bytes with a newer format number, wrapped in save.c's CRC */
  gc_system_init_defaults(&S2);
  uint8_t* buf = malloc(1024 * 1024);
  const size_t n = gc_system_save(&S2, buf, 1024 * 1024);
  CHECK(n > 20, "serialized %zu", n);
  buf[8] = 0xff;
  buf[9] = 0x7f; /* format 32767 */
  remove(p);
  CHECK(save_write("system", buf, n) == SAVE_OK, "write the newer file");
  const size_t newer = file_size(p);
  sysd_reset();
  CHECK(sysd_state(why) == SYSD_UNREADABLE && strstr(why, "newer"), "newer version: \"%s\"", why);
  sysd_get()->dex[1].seen_count = 9;
  CHECK(sysd_save() && file_size(p) == newer, "newer version: nothing to write, and not written over");
  free(buf);

  /* back to a fresh folder state for the next tests */
  remove(p);
  remove(bak);
  sysd_reset();
  CHECK(sysd_state(NULL) == SYSD_NEW, "removing the file makes it a new install again");
}

/* ------------------------------------------------------------------ the list, the panel, the team */

static int cmp_u16(const void* a, const void* b) { return (int)*(const uint16_t*)a - (int)*(const uint16_t*)b; }

static void test_new_save(void) {
  gc_system_init_defaults(&S1);
  static sm_entry list[SS_MAX_ENTRIES];
  int refused_n = -1;
  const int n = ss_list(&S1, list, SS_MAX_ENTRIES, &refused_n);
  CHECK(refused_n == 0, "a new save refuses nothing");
  CHECK(n == gc_default_starter_count, "a new save lists %d species, the defaults are %d", n, gc_default_starter_count);
  uint16_t sorted[64];
  memcpy(sorted, gc_default_starters, sizeof(uint16_t) * (size_t)gc_default_starter_count);
  qsort(sorted, (size_t)gc_default_starter_count, sizeof sorted[0], cmp_u16);
  for (int i = 0; i < n && i < gc_default_starter_count; i++) {
    CHECK(list[i].species == sorted[i], "entry %d is species %u, expected %u", i, list[i].species, sorted[i]);
    CHECK(list[i].cost == gc_starter_value(&S1, list[i].species), "cost of %u", list[i].species);
    CHECK(list[i].base_cost == (double)gc_starter_cost(list[i].species), "base cost of %u", list[i].species);
    CHECK(!list[i].shiny && !list[i].passive && !list[i].hidden_ability && !list[i].won, "%u starts plain", list[i].species);
  }
  CHECK(ss_list(NULL, list, SS_MAX_ENTRIES, NULL) == 0, "no system, no list");
  CHECK(ss_list(&S1, list, 3, NULL) == 3, "the list stops at the capacity");

  /* the panel of the first species: Bulbasaur's line is a default starter */
  starter_info info;
  const uint16_t first = list[0].species;
  ss_describe(&S1, first, &info);
  const pr_species* sp = menu_species(first);
  const pr_form* f = &pr_forms[sp->form0];
  CHECK(info.mon.species == first && !strcmp(info.mon.name, sp->name), "name %s", info.mon.name);
  CHECK(info.mon.ability == f->ability1 && !info.ability_hidden, "the first ability");
  CHECK(info.mon.types[0] == f->type1 && info.mon.types[1] == f->type2, "types");
  CHECK(info.mon.nature < 25 && (S1.dex[first].nature_attr & (1u << (info.mon.nature + 1))), "a nature the dex holds");
  uint16_t mv[4] = {0};
  const int nm = gc_starter_default_moves(first, mv);
  CHECK(nm > 0 && !memcmp(mv, info.moves, sizeof mv), "the default moves (%d)", nm);
  CHECK(!info.passive_unlocked && !info.passive_enabled, "passive locked");

  /* a team of the first three that fit */
  uint16_t pick[3];
  int k = 0;
  double total = 0;
  for (int i = 0; i < n && k < 3; i++)
    if (total + list[i].cost <= gc_starter_budget(GAME_MODES_CLASSIC)) {
      pick[k++] = list[i].species;
      total += list[i].cost;
    }
  gc_starter team[6];
  const char* why = NULL;
  CHECK(ss_team(&S1, pick, k, team, &why), "a legal team: %s", why ? why : "");
  for (int i = 0; i < k; i++) {
    CHECK(team[i].species == pick[i] && team[i].form == 0 && team[i].ability_index == 0, "member %d", i);
    CHECK(team[i].tera_type == -1 && !team[i].passive && !team[i].shiny && team[i].pokerus==rotating_carrier(pick[i]), "member %d defaults", i);
    for (int j = 0; j < 6; j++) CHECK(team[i].ivs[j] == 15, "member %d iv %d = %d", i, j, team[i].ivs[j]);
    uint16_t expected[4]={0};gc_starter_default_moves(pick[i],expected);
    for(int j=0;j<4;j++)CHECK(team[i].moves[j]==expected[j],"member %d explicit default move %d",i,j);
  }
  CHECK(core_takes(&S1, team, k, &why), "game-core takes the team: %s", why ? why : "");
  CHECK(RUN.n_party == k && RUN.party[0].b.species == pick[0], "the party is the team");

  /* what is not a team */
  uint16_t two[2] = {pick[0], pick[0]};
  CHECK(!ss_team(&S1, two, 2, team, &why), "the same species twice");
  CHECK(!ss_team(&S1, pick, 0, team, &why) && !strcmp(why, "starterSelectUiHandler:invalidParty"), "an empty team");
  uint16_t seven[7];
  for (int i = 0; i < 7; i++) seven[i] = pick[0];
  CHECK(!ss_team(&S1, seven, 7, team, &why), "seven");
  uint16_t unknown[1] = {150}; /* Mewtwo: a starter, never caught in a new save */
  CHECK(!ss_team(&S1, unknown, 1, team, &why), "a species the dex lacks: %s", why ? why : "");
  unknown[0] = 2; /* Ivysaur: not a starter */
  CHECK(!ss_team(&S1, unknown, 1, team, &why), "not a starter species");
  gc_starter one;
  CHECK(!ss_starter(&S1, 0, &one) && !ss_starter(&S1, 2, &one), "ss_starter of a non-starter");
}

static void test_everything_unlocked(void) {
  unlock_all(&S1);
  static sm_entry list[SS_MAX_ENTRIES];
  int refused_n = -1;
  const int n = ss_list(&S1, list, SS_MAX_ENTRIES, &refused_n);
  CHECK(refused_n == 0, "everything unlocked refuses nothing: %d", refused_n);
  CHECK(n == gc_starter_cost_count, "everything unlocked lists %d of %d starters", n, gc_starter_cost_count);
  for (int i = 1; i < n; i++) CHECK(list[i - 1].species < list[i].species, "ascending at %d", i);
  int refused = 0;
  const char* why = NULL;
  for (int i = 0; i < n; i++) {
    gc_starter team[6];
    uint16_t sp = list[i].species;
    if (!ss_team(&S1, &sp, 1, team, &why)) {
      refused++;
      if (refused < 10) printf("  refused alone: species %u: %s\n", sp, why ? why : "?");
      continue;
    }
    if (!core_takes(&S1, team, 1, &why)) {
      refused++;
      if (refused < 10) printf("  game-core refused species %u: %s\n", sp, why ? why : "?");
    }
  }
  CHECK(refused == 0, "%d of %d species could not start a run alone", refused, n);

  /* the cost budget: the cheapest species fill six places, a dear one does not fit with them */
  uint16_t cheap[6], dear = 0;
  int nc = 0;
  for (int i = 0; i < n; i++) {
    if (list[i].cost <= 1 && nc < 6) cheap[nc++] = list[i].species;
    if (list[i].cost >= 8 && !dear) dear = list[i].species;
  }
  gc_starter team[6];
  CHECK(nc == 6 && ss_team(&S1, cheap, 6, team, &why), "six cheap species");
  uint16_t mix[6];
  for (int i = 0; i < 5; i++) mix[i] = cheap[i];
  mix[5] = dear;
  double total = 0;
  for (int i = 0; i < 6; i++) total += gc_starter_value(&S1, mix[i]);
  CHECK(total > 10 ? !ss_team(&S1, mix, 6, team, &why) : ss_team(&S1, mix, 6, team, &why), "total %.1f against the budget", total);
  if (total > 10) CHECK(!strcmp(why, "starterSelectUiHandler:cost"), "the reason is the cost: %s", why);
}

/* The known disagreement with upstream, on its own: a prevolution reached only through an evolved form with a form of its own. */
static void test_prevolution_without_form_bit(void) {
  gc_system_init_defaults(&S1);
  const uint16_t litleo = 667, pyroar = 668;
  const pr_species* sp = menu_species(pyroar);
  CHECK(sp && sp->nforms > 1 && !pr_forms[sp->form0 + 1].unobtainable, "Pyroar has an obtainable second form");
  gc_mon m = make_mon(pyroar, 1, false, 0, 0, 3);
  m.b.gender = 1;
  gc_system_set_pokemon_caught(&S1, &m, true, false);
  printf("  after a female Pyroar: Pyroar caught %llx, Litleo caught %llx\n", (unsigned long long)S1.dex[pyroar].caught_attr,
         (unsigned long long)S1.dex[litleo].caught_attr);
  CHECK(S1.dex[litleo].caught_attr != 0 && (S1.dex[litleo].caught_attr >> 7) == 0, "Litleo is caught with no form bit (upstream's masking)");
  static sm_entry list[SS_MAX_ENTRIES];
  int refused_n = 0;
  const int n = ss_list(&S1, list, SS_MAX_ENTRIES, &refused_n);
  bool has_litleo = false;
  for (int i = 0; i < n; i++) has_litleo |= list[i].species == litleo;
  CHECK(!has_litleo && refused_n == 1, "Litleo is left out (game-core refuses its form 0): left out %d", refused_n);
  const char* why = NULL;
  CHECK(!gc_starter_is_available(&S1, &(gc_starter){.species = litleo, .tera_type = -1}, NULL, &why) && !strcmp(why, "starterSelectUiHandler:lockedForm"),
        "game-core's verdict on Litleo: %s", why ? why : "?");
}

/* ------------------------------------------------------------------ the fuzz */

static void test_random_systems(int systems) {
  static sm_entry list[SS_MAX_ENTRIES];
  long listed = 0, alone = 0, teams = 0, excluded = 0;
  int refused = 0;
  for (int s = 0; s < systems; s++) {
    random_system(&S1, (int)rnd(400));
    int dropped = 0;
    const int n = ss_list(&S1, list, SS_MAX_ENTRIES, &dropped);
    listed += n;
    excluded += dropped;
    /* the species left out are exactly the known case: caught, and no form bit at all */
    int found = 0, k = 0;
    for (int c = 0; c < gc_starter_cost_count; c++) {
      const uint16_t id = gc_starter_costs[c].id;
      while (k < n && list[k].species < id) k++;
      if (k < n && list[k].species == id) continue;
      const uint64_t a = S1.dex[id].caught_attr;
      if (!((a & 1) || ((a & 2) && (a & 0x70)))) continue; /* not caught */
      found++;
      gc_starter one;
      const char* why = NULL;
      const uint64_t forms_it_has = ((1ull << menu_species(id)->nforms) - 1) << 7;
      CHECK(ss_starter(&S1, id, &one) && !gc_starter_is_available(&S1, &one, NULL, &why) && !strcmp(why, "starterSelectUiHandler:lockedForm") &&
                (a & forms_it_has) == 0,
            "species %u left out of the list for another reason: %s (caught %llx)", id, why ? why : "?", (unsigned long long)a);
    }
    CHECK(found == dropped, "left out %d, counted %d", found, dropped);
    for (int i = 0; i < n; i++) {
      CHECK(list[i].species > 0 && (i == 0 || list[i - 1].species < list[i].species), "ascending");
      gc_starter team[6];
      const char* why = NULL;
      uint16_t sp = list[i].species;
      alone++;
      if (!ss_team(&S1, &sp, 1, team, &why) || !core_takes(&S1, team, 1, &why)) {
        refused++;
        if (refused < 12)
          printf("  system %d: species %u (%s) listed but refused alone: %s (caught %llx, full unlocks %llx, %d forms)\n", s, sp,
                 menu_species(sp)->name, why ? why : "?", (unsigned long long)S1.dex[sp].caught_attr,
                 (unsigned long long)gc_species_full_unlocks_data(sp), menu_species(sp)->nforms);
      }
    }
    /* random teams within the budget */
    for (int t = 0; t < 20 && n > 0; t++) {
      uint16_t pick[6];
      int k = 0;
      double total = 0;
      for (int tries = 0; tries < 30 && k < 6; tries++) {
        const sm_entry* e = &list[rnd((uint32_t)n)];
        bool dup = false;
        for (int j = 0; j < k; j++) dup |= pick[j] == e->species;
        if (dup || total + e->cost > gc_starter_budget(GAME_MODES_CLASSIC)) continue;
        pick[k++] = e->species;
        total += e->cost;
      }
      if (!k) continue;
      gc_starter team[6];
      const char* why = NULL;
      teams++;
      if (!ss_team(&S1, pick, k, team, &why) || !core_takes(&S1, team, k, &why)) {
        refused++;
        if (refused < 12) printf("  system %d: a team of %d within the budget was refused: %s\n", s, k, why ? why : "?");
      }
    }
  }
  printf("  %d random systems: %ld species listed (%ld caught species left out for having no bit of a form they have), %ld tried alone, %ld teams, %d refused\n",
         systems, listed, excluded, alone, teams, refused);
  CHECK(refused == 0, "%d teams the screen would offer were refused", refused);
}

static void test_pokerus_rotation(void){
 unlock_all(&S1);
 for(int epoch=0;epoch<64;epoch++){
  starter_clock=1792627200+epoch*10800;
  sm_entry entries[SS_MAX_ENTRIES];int n=ss_list(&S1,entries,SS_MAX_ENTRIES,NULL),count=0;
  CHECK(n==572,"all starters listed");
  for(int i=0;i<n;i++){
   uint16_t species=entries[i].species;bool expected=rotating_carrier(species);
   gc_starter starter;starter_info info;ss_describe(&S1,species,&info);
   CHECK(ss_starter(&S1,species,&starter),"starter %u available",species);
   CHECK(entries[i].pokerus==expected&&starter.pokerus==expected&&info.mon.pokerus==expected,"rotation reaches grid, description and chosen starter %u",species);
   if(expected){count++;gc_run_new(&RUN,GAME_MODES_CLASSIC,"pokerus-visible",1);const char *why=NULL;
    CHECK(gc_run_set_starters(&RUN,&starter,1,&why)&&RUN.party[0].pokerus,"infection reaches real run %u",species);}
  }
  CHECK(count==3,"three infected starters each three-hour epoch");
 }
 starter_clock=1792627200;
}

/* ------------------------------------------------------------------ the filter and sort (source/starter_filter.c) */

static gc_system FS;
static sm_entry g_all[SS_MAX_ENTRIES], g_out[SS_MAX_ENTRIES];

static int apply(const ssf* f) {
  const int n_all = ss_list(&FS, g_all, SS_MAX_ENTRIES, NULL);
  return ssf_apply(&FS, f, g_all, n_all, g_out);
}
static bool in_out(int n, uint16_t species) {
  for (int i = 0; i < n; i++)
    if (g_out[i].species == species) return true;
  return false;
}
static int type_has(uint16_t species, int t) {
  const pr_species* sp = menu_species(species);
  const pr_form* f = &pr_forms[sp->form0];
  return f->type1 == t || f->type2 == t;
}

static void test_filter(void) {
  unlock_all(&FS);
  ssf f;
  ssf_reset(&f);
  const int n_all = ss_list(&FS, g_all, SS_MAX_ENTRIES, NULL);
  CHECK(n_all > 100 && ssf_active(&f) == 0, "a full dex lists %d species and the default filter is no filter", n_all);
  CHECK(apply(&f) == n_all, "the default filter keeps every species");

  /* generation */
  f.gens = 1u << 0;
  int n = apply(&f);
  bool all_gen1 = n > 0 && n < n_all, none_missed = true;
  for (int i = 0; i < n; i++) all_gen1 = all_gen1 && menu_species(g_out[i].species)->generation == 1;
  for (int i = 0; i < n_all; i++)
    if (menu_species(g_all[i].species)->generation == 1 && !in_out(n, g_all[i].species)) none_missed = false;
  CHECK(all_gen1 && none_missed && in_out(n, 1) && !in_out(n, 152), "generation 1 only: %d species, Bulbasaur in, Chikorita out", n);
  CHECK(ssf_active(&f) == 1, "one column changed");
  f.gens = (1u << 0) | (1u << 1);
  CHECK(in_out(apply(&f), 152), "generations 1 and 2 keep Chikorita");
  f.gens = SSF_GENS_ALL;

  /* type: either slot counts */
  f.types = 1u << POKEMON_TYPE_FIRE;
  n = apply(&f);
  bool fire = n > 0;
  for (int i = 0; i < n; i++) fire = fire && type_has(g_out[i].species, POKEMON_TYPE_FIRE);
  for (int i = 0; i < n_all; i++)
    if (type_has(g_all[i].species, POKEMON_TYPE_FIRE) && !in_out(n, g_all[i].species)) fire = false;
  CHECK(fire && in_out(n, 4) && !in_out(n, 1), "fire only: %d species, Charmander in, Bulbasaur out", n);
  f.types = (1u << POKEMON_TYPE_GRASS);
  CHECK(in_out(apply(&f), 1), "Bulbasaur is grass (its first type)");
  f.types = (1u << POKEMON_TYPE_POISON);
  CHECK(in_out(apply(&f), 1), "and poison (its second)");
  f.types = SSF_TYPES_ALL;

  /* rarity (beyond upstream) */
  f.rarity = SSF_R_LEGENDARY;
  n = apply(&f);
  bool leg = n > 0;
  for (int i = 0; i < n; i++) leg = leg && menu_species(g_out[i].species)->rarity == 2;
  CHECK(leg && in_out(n, 150), "legendary only: %d species, Mewtwo in", n);
  f.rarity = SSF_R_MYTHICAL;
  CHECK(in_out(apply(&f), 251) && !in_out(n, 1), "mythical only has Celebi");
  f.rarity = SSF_R_ORDINARY;
  n = apply(&f);
  CHECK(in_out(n, 1) && !in_out(n, 150) && !in_out(n, 251), "ordinary only has Bulbasaur, not Mewtwo or Celebi");
  f.rarity = SSF_R_ALL;

  /* caught: the highest tier caught decides */
  const uint64_t base = 128ull; /* form 0 */
  FS.dex[4].caught_attr = base | 1ull;                          /* normal only */
  FS.dex[7].caught_attr = base | 1ull | 2ull | 16ull;           /* normal + shiny tier 1 */
  FS.dex[25].caught_attr = base | 2ull | 16ull | 32ull;         /* shiny tiers 1 and 2 */
  FS.dex[1].caught_attr = base | 2ull | 16ull | 32ull | 64ull;  /* all three tiers */
  f.caught = SSF_C_NORMAL;
  CHECK(ssf_fits(&FS, &f, 4) && !ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 25) && !ssf_fits(&FS, &f, 1), "Normal: only a plain catch");
  f.caught = SSF_C_SHINY1;
  CHECK(!ssf_fits(&FS, &f, 4) && ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 25) && !ssf_fits(&FS, &f, 1), "Shiny: tier 1 as the best");
  f.caught = SSF_C_SHINY2;
  CHECK(ssf_fits(&FS, &f, 25) && !ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 1), "Rare shiny: tier 2 as the best");
  f.caught = SSF_C_SHINY3;
  CHECK(ssf_fits(&FS, &f, 1) && !ssf_fits(&FS, &f, 25), "Epic shiny: tier 3");
  f.caught = SSF_C_SHINY3 | SSF_C_NORMAL;
  CHECK(ssf_fits(&FS, &f, 1) && ssf_fits(&FS, &f, 4) && !ssf_fits(&FS, &f, 7), "two tiers together");
  f.caught = SSF_C_ALL;

  /* unlocks and misc, on species with known data */
  unlock_all(&FS);
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    FS.starters[id].passive_attr = 0;
    FS.starters[id].value_reduction = 0;
    FS.starters[id].candy_count = 0;
    FS.starters[id].classic_win_count = 0;
    FS.starters[id].ability_attr = 3;
  }
  FS.starters[4].passive_attr = 3;
  FS.starters[7].value_reduction = 1;
  FS.starters[25].value_reduction = 2;
  FS.starters[1].classic_win_count = 2;
  FS.starters[1].ability_attr = 7;
  f.passive = SSF_ON;
  CHECK(ssf_fits(&FS, &f, 4) && !ssf_fits(&FS, &f, 7), "passive unlocked");
  f.passive = SSF_UNLOCKABLE;
  CHECK(!ssf_fits(&FS, &f, 7), "no candies: the passive is not unlockable");
  FS.starters[7].candy_count = 1000;
  CHECK(ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 4), "with candies it is, unless it is already unlocked");
  f.passive = SSF_OFF;
  f.reduction = SSF_ONE;
  CHECK(ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 25) && !ssf_fits(&FS, &f, 4), "bought once");
  f.reduction = SSF_TWO;
  CHECK(ssf_fits(&FS, &f, 25) && !ssf_fits(&FS, &f, 7), "bought twice");
  f.reduction = SSF_ON;
  CHECK(ssf_fits(&FS, &f, 25) && ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 4), "bought at all");
  f.reduction = SSF_UNLOCKABLE;
  CHECK(ssf_fits(&FS, &f, 7) == (gc_system_starter_reduction_cost(&FS, 7) >= 0 && 1000 >= gc_system_starter_reduction_cost(&FS, 7)), "reduction unlockable follows game-core's price");
  CHECK(!ssf_fits(&FS, &f, 25), "nothing left to buy after two");
  f.reduction = SSF_OFF;
  f.win = SSF_ON;
  CHECK(ssf_fits(&FS, &f, 1) && !ssf_fits(&FS, &f, 4), "has won");
  f.win = SSF_EXCLUDE;
  CHECK(!ssf_fits(&FS, &f, 1) && ssf_fits(&FS, &f, 4), "has not won");
  f.win = SSF_OFF;
  f.hidden_ability = SSF_ON;
  CHECK(ssf_fits(&FS, &f, 1) && !ssf_fits(&FS, &f, 4), "hidden ability unlocked");
  f.hidden_ability = SSF_EXCLUDE;
  CHECK(!ssf_fits(&FS, &f, 1) && ssf_fits(&FS, &f, 4), "hidden ability available but not unlocked");
  f.hidden_ability = SSF_OFF;
  f.egg = SSF_ON;
  CHECK(ssf_fits(&FS, &f, 7) && !ssf_fits(&FS, &f, 4), "an egg of its own species can be bought (1000 candies against none)");
  f.egg = SSF_OFF;
  starter_clock += 0;
  uint16_t carriers[3];
  gc_pokerus_starters((uint64_t)starter_clock * 1000, carriers);
  f.pokerus = SSF_ON;
  CHECK(ssf_fits(&FS, &f, carriers[0]) && !ssf_fits(&FS, &f, carriers[0] == 4 ? 7 : 4), "Pokerus carriers");
  f.pokerus = SSF_EXCLUDE;
  CHECK(!ssf_fits(&FS, &f, carriers[0]), "and without it");

  /* sort */
  unlock_all(&FS);
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    FS.starters[id].candy_count = (int32_t)rnd(500);
    FS.dex[id].caught_count = rnd(300);
    FS.dex[id].hatched_count = rnd(300);
  }
  ssf_reset(&f);
  const int total = ss_list(&FS, g_all, SS_MAX_ENTRIES, NULL);
  for (int dir = 0; dir < 2; dir++) {
    f.descending = dir;
    for (int sort = 0; sort < SSF_SORT_COUNT; sort++) {
      f.sort = (uint8_t)sort;
      n = apply(&f);
      bool ordered = n == total;
      for (int i = 1; i < n && ordered; i++) {
        const uint16_t a = g_out[i - 1].species, b = g_out[i].species;
        double ka = 0, kb = 0;
        switch (sort) {
          case SSF_SORT_NUMBER: ka = a; kb = b; break;
          case SSF_SORT_COST: ka = g_out[i - 1].cost; kb = g_out[i].cost; break;
          case SSF_SORT_CANDY: ka = FS.starters[a].candy_count; kb = FS.starters[b].candy_count; break;
          case SSF_SORT_IV: {
            for (int k = 0; k < 6; k++) { ka += FS.dex[a].ivs[k]; kb += FS.dex[b].ivs[k]; }
            break;
          }
          case SSF_SORT_NAME: {
            char na[64];
            snprintf(na, sizeof na, "%s", i18n_species_name(a));
            ka = strcmp(na, i18n_species_name(b)) < 0 ? 0 : strcmp(na, i18n_species_name(b)) > 0 ? 1 : 0.5;
            kb = 0.5; /* 0 = a before b, 1 = after */
            ordered = ordered && (dir ? ka >= 0.5 : ka <= 0.5);
            continue;
          }
          case SSF_SORT_CAUGHT: ka = FS.dex[a].caught_count; kb = FS.dex[b].caught_count; break;
          case SSF_SORT_HATCHED: ka = FS.dex[a].hatched_count; kb = FS.dex[b].hatched_count; break;
        }
        if (dir ? ka < kb : ka > kb) ordered = false;
        if (ka == kb && a > b) ordered = false; /* ties: species number, ascending */
      }
      CHECK(ordered, "sort %d, %s: %d species in order", sort, dir ? "descending" : "ascending", n);
    }
  }
  f.sort = SSF_SORT_NUMBER;
  f.descending = true;
  apply(&f);
  CHECK(g_out[0].species > g_out[1].species, "number, descending: the last species first");
  CHECK(ssf_active(&f) == 1, "the order counts as one column");
  ssf_reset(&f);
  CHECK(ssf_active(&f) == 0, "reset");
}

int main(int argc, char** argv) {
  const int systems = argc > 1 ? atoi(argv[1]) : 300;
  char dir[64] = "/tmp/starter-data-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  test_pokerus_rotation();
  test_system_data();
  test_new_save();
  test_everything_unlocked();
  test_prevolution_without_form_bit();
  test_random_systems(systems);
  test_filter();

  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  if (chdir("/") == 0) {
    char cmd[128];
    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) printf("(could not remove %s)\n", dir);
  }
  return failed ? 1 : 0;
}
