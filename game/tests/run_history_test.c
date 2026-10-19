/* run_history_test: the run history on the SD card (run_history.c, history_record.c) with the real save.c, on the host.
 *
 *   build/run_history_test
 *
 * Works in a temporary folder (the SD card's "sdmc:/..." paths are relative here). Checks: an empty history, entries written and read
 * back field by field, newest first, the limit of 25 (the oldest goes), the byte format (every cut of a file and every flipped bit is
 * refused or read without a crash; the longest entry fits RH_ENTRY_MAX), a file that does not decode is never overwritten and the run
 * that ended meanwhile is not added, a failed write leaves the history as it was, and the entry built from a run's end state. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gc/run.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "run_history.h"
#include "save.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      printf("FAIL line %d: %s: ", __LINE__, #cond);      \
      printf(__VA_ARGS__);                                \
      printf("\n");                                       \
    }                                                     \
  } while (0)

static rh_mon mon(int seed) {
  rh_mon m;
  memset(&m, 0, sizeof m);
  m.species = (uint16_t)(1 + seed * 7 % 900);
  m.form = (uint8_t)(seed % 3);
  m.gender = (int8_t)(seed % 3 - 1);
  m.shiny = seed % 5 == 0;
  m.variant = (uint8_t)(seed % 3);
  m.level = (uint32_t)(5 + seed * 13);
  m.hp = seed % 4 == 0 ? 0 : (uint32_t)(10 + seed);
  m.fainted = m.hp == 0;
  m.nature = (uint8_t)(seed % 25);
  m.ability = (uint16_t)(1 + seed % 200);
  m.passive = (uint16_t)(seed % 100);
  m.has_passive = seed % 2 == 0;
  m.n_moves = (uint8_t)(1 + seed % 4);
  for (int i = 0; i < m.n_moves; i++) {
    m.moves[i] = (uint16_t)(1 + (seed + i) % 700);
    m.pp_used[i] = (uint8_t)(i + seed % 3);
    m.pp_max[i] = (uint8_t)(10 + i * 5);
  }
  m.types[0] = (int8_t)(seed % 18);
  m.types[1] = seed % 2 ? (int8_t)((seed + 3) % 18) : -1;
  for (int i = 0; i < 6; i++) {
    m.stats[i] = (uint32_t)(20 + seed + i * 11);
    m.ivs[i] = (uint8_t)((seed + i) % 32);
  }
  m.friendship = (uint8_t)(seed % 256);
  m.tera_type = (int8_t)(seed % 19 - 1);
  m.luck = (uint8_t)(seed % 4);
  return m;
}

static rh_entry entry(int seed) {
  rh_entry e;
  memset(&e, 0, sizeof e);
  e.time = 1760000000 + seed * 3600;
  e.victory = seed % 3 == 0;
  e.mode = seed % 5;
  e.wave = 1 + seed * 7;
  e.biome = seed % 40 - 1;
  e.score = seed * 1234;
  e.money = (int64_t)seed * 100003;
  e.n_party = (uint8_t)(1 + seed % 6);
  for (int i = 0; i < e.n_party; i++) e.party[i] = mon(seed + i);
  if (!e.victory) {
    e.defeat = (uint8_t)(1 + seed % 3); /* wild, trainer, rival */
    if (e.defeat == RH_DEFEAT_WILD) {
      e.n_enemy = (uint8_t)(1 + seed % 2);
      for (int i = 0; i < e.n_enemy; i++) e.enemy[i] = mon(seed + 50 + i);
    } else {
      snprintf(e.trainer, sizeof e.trainer, "$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.%d)", seed % 8);
      e.trainer_double = seed % 2 == 0;
      e.trainer_female = seed % 4 == 0;
    }
  }
  if (seed % 2) snprintf(e.name, sizeof e.name, "Run %d \xc3\xa9", seed);
  return e;
}

static bool same_mon(const rh_mon* a, const rh_mon* b) { return !memcmp(a, b, sizeof *a); }

/* field by field, not memcmp: the structs have padding */
static bool same(const rh_entry* a, const rh_entry* b) {
  if (a->time != b->time || a->victory != b->victory || a->mode != b->mode || a->wave != b->wave || a->biome != b->biome ||
      a->score != b->score || a->money != b->money || a->n_party != b->n_party || a->defeat != b->defeat || a->n_enemy != b->n_enemy ||
      a->trainer_double != b->trainer_double || a->trainer_female != b->trainer_female || strcmp(a->trainer, b->trainer) ||
      strcmp(a->name, b->name))
    return false;
  for (int i = 0; i < a->n_party; i++)
    if (!same_mon(&a->party[i], &b->party[i])) return false;
  for (int i = 0; i < a->n_enemy; i++)
    if (!same_mon(&a->enemy[i], &b->enemy[i])) return false;
  return true;
}

static void write_raw(const char* path, const void* data, size_t n) {
  FILE* f = fopen(path, "wb");
  if (f) {
    fwrite(data, 1, n, f);
    fclose(f);
  }
}

static bool read_raw(const char* path, uint8_t** out, size_t* len) {
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  *len = (size_t)ftell(f);
  fseek(f, 0, SEEK_SET);
  *out = malloc(*len + 1);
  const bool ok = fread(*out, 1, *len, f) == *len;
  fclose(f);
  return ok;
}

static rh_entry g_dec[RH_LIMIT];
static rh_entry g_in[RH_LIMIT];
static uint8_t g_buf[RH_FILE_MAX];

int main(void) {
  char dir[64] = "/tmp/run-history-test-XXXXXX";
  if (!mkdtemp(dir) || chdir(dir) != 0 || mkdir("sdmc:", 0777) != 0 || !save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  const char* path = SAVE_DIR "/run_history.sav";

  /* ---- no file: an empty history */
  CHECK(rhist_load() == RH_OK && rhist_count() == 0, "an empty history");
  CHECK(rhist_get(0) == NULL && rhist_get(-1) == NULL, "no entry 0");

  /* ---- the byte format round-trips every field */
  for (int i = 0; i < 12; i++) g_in[i] = entry(i);
  size_t len = rhist_encode(g_in, 12, g_buf, sizeof g_buf);
  CHECK(len > 0 && len <= 8 + 12 * RH_ENTRY_MAX, "encoded %zu bytes", len);
  CHECK(rhist_decode(g_buf, len, g_dec, RH_LIMIT) == 12, "decode count");
  for (int i = 0; i < 12; i++) CHECK(same(&g_in[i], &g_dec[i]), "entry %d differs after the round trip", i);
  /* the longest entry: a full party of fainted level 10000s, two enemies, the longest strings */
  rh_entry big = entry(5);
  big.victory = false;
  big.defeat = RH_DEFEAT_WILD;
  big.n_party = RH_PARTY;
  big.n_enemy = RH_ENEMIES;
  for (int i = 0; i < RH_PARTY; i++) {
    big.party[i] = mon(i + 1);
    big.party[i].level = 0xffffffffu;
    for (int k = 0; k < 6; k++) big.party[i].stats[k] = 0xffffffffu;
    big.party[i].n_moves = 4;
  }
  big.enemy[0] = mon(9);
  big.enemy[1] = mon(10);
  memset(big.trainer, 'x', sizeof big.trainer - 1);
  memset(big.name, 'y', sizeof big.name - 1);
  big.time = INT64_MAX;
  big.money = INT64_MAX;
  big.wave = 0x7fffffff;
  big.score = -5;
  len = rhist_encode(&big, 1, g_buf, sizeof g_buf);
  CHECK(len > 0 && len <= 8 + RH_ENTRY_MAX, "the longest entry is %zu bytes (limit %d)", len, 8 + RH_ENTRY_MAX);
  CHECK(rhist_decode(g_buf, len, g_dec, RH_LIMIT) == 1 && same(&big, &g_dec[0]), "the longest entry round-trips");
  /* an entry the format can't hold is refused, not cut */
  rh_entry bad = entry(1);
  bad.n_party = 7;
  CHECK(rhist_encode(&bad, 1, g_buf, sizeof g_buf) == 0, "7 party members");
  bad = entry(1);
  memset(bad.trainer, 'x', sizeof bad.trainer); /* no NUL */
  CHECK(rhist_encode(&bad, 1, g_buf, sizeof g_buf) == 0, "a trainer string without its NUL");
  CHECK(rhist_encode(g_in, 26, g_buf, sizeof g_buf) == 0 && rhist_encode(g_in, -1, g_buf, sizeof g_buf) == 0, "more than the limit");
  CHECK(rhist_encode(g_in, 12, g_buf, 100) == 0, "a buffer that is too small");

  /* ---- every cut of a file, and every flipped bit, is refused or decodes to something valid (ASan/UBSan watch the rest) */
  len = rhist_encode(g_in, 3, g_buf, sizeof g_buf);
  static uint8_t cut[RH_FILE_MAX];
  int accepted_cuts = 0;
  for (size_t n = 0; n < len; n++) {
    memcpy(cut, g_buf, n);
    if (rhist_decode(cut, n, g_dec, RH_LIMIT) >= 0) accepted_cuts++;
  }
  CHECK(accepted_cuts == 0, "%d cut files were accepted", accepted_cuts);
  CHECK(rhist_decode(g_buf, len + 1, g_dec, RH_LIMIT) == -1, "a byte after the last entry");
  int flips_ok = 0;
  for (size_t i = 0; i < len; i++)
    for (int b = 0; b < 8; b++) {
      memcpy(cut, g_buf, len);
      cut[i] ^= (uint8_t)(1u << b);
      const int n = rhist_decode(cut, len, g_dec, RH_LIMIT);
      if (n >= 0)
        for (int k = 0; k < n; k++) CHECK(g_dec[k].n_party <= RH_PARTY && g_dec[k].n_enemy <= RH_ENEMIES && g_dec[k].defeat <= RH_DEFEAT_RIVAL, "a flipped bit gave a bad entry");
      flips_ok++;
    }
  CHECK(flips_ok == (int)len * 8, "flips");
  /* wrong magic, wrong version, a count over the limit, control characters in a string */
  memcpy(cut, g_buf, len);
  cut[0] = 'X';
  CHECK(rhist_decode(cut, len, g_dec, RH_LIMIT) == -1, "wrong magic");
  memcpy(cut, g_buf, len);
  cut[3] = 2;
  CHECK(rhist_decode(cut, len, g_dec, RH_LIMIT) == -1, "wrong version");
  memcpy(cut, g_buf, len);
  cut[4] = 26;
  CHECK(rhist_decode(cut, len, g_dec, RH_LIMIT) == -1, "26 entries");
  CHECK(rhist_decode(g_buf, len, g_dec, 2) == -1, "a caller buffer smaller than the count");

  /* ---- adding: newest first, persisted, 25 at most (the oldest goes) */
  rh_status st = RH_UNREADABLE;
  rh_entry e;
  for (int i = 0; i < 4; i++) {
    e = entry(100 + i);
    CHECK(rhist_add(&e, &st) && st == RH_OK, "add %d", i);
    CHECK(rhist_count() == i + 1, "count %d after add %d", rhist_count(), i);
    CHECK(rhist_get(0) && same(rhist_get(0), &e), "the newest is the one just added");
  }
  CHECK(rhist_load() == RH_OK && rhist_count() == 4, "read back %d", rhist_count());
  for (int i = 0; i < 4; i++) {
    const rh_entry want = entry(103 - i);
    CHECK(rhist_get(i) && same(rhist_get(i), &want), "entry %d from the newest", i);
  }
  CHECK(rhist_get(4) == NULL, "no entry past the last");
  for (int i = 4; i < 30; i++) {
    e = entry(100 + i);
    CHECK(rhist_add(&e, NULL), "add %d", i);
  }
  CHECK(rhist_count() == RH_LIMIT, "the limit holds: %d", rhist_count());
  CHECK(rhist_load() == RH_OK && rhist_count() == RH_LIMIT, "the limit holds on disk: %d", rhist_count());
  { /* 30 were added in all: the newest 25 (105..129) stay, in order */
    bool order = true;
    for (int i = 0; i < RH_LIMIT; i++) {
      const rh_entry want = entry(100 + 29 - i);
      if (!rhist_get(i) || !same(rhist_get(i), &want)) order = false;
    }
    CHECK(order, "the newest 25 stay, newest first");
  }

  /* ---- a file that does not decode: never overwritten, the run is not added, the history is empty on screen */
  uint8_t* before;
  size_t before_len;
  CHECK(read_raw(path, &before, &before_len), "read the file");
  uint8_t* junk = malloc(before_len);
  memcpy(junk, before, before_len);
  junk[10] ^= 0x55;      /* the CRC no longer matches */
  write_raw(path, junk, before_len);
  unlink(SAVE_DIR "/run_history.sav.bak"); /* and no backup to fall back to */
  CHECK(rhist_load() == RH_UNREADABLE && rhist_count() == 0, "a damaged file is unreadable");
  e = entry(7);
  CHECK(!rhist_add(&e, &st) && st == RH_UNREADABLE, "no add to an unreadable history");
  uint8_t* after;
  size_t after_len;
  CHECK(read_raw(path, &after, &after_len) && after_len == before_len && !memcmp(after, junk, before_len), "the unreadable file was not touched");
  CHECK(access(SAVE_DIR "/run_history.sav.bak", F_OK) != 0, "no backup was made of it");
  free(after);
  /* a file with a good CRC that is not a history (another version's) is the same */
  CHECK(save_write("run_history", "RHI\x09zzzzzzzz", 12) == SAVE_OK, "write a foreign file");
  CHECK(rhist_load() == RH_UNREADABLE, "a newer version's file is unreadable");
  CHECK(!rhist_add(&e, &st) && st == RH_UNREADABLE, "no add to it either");
  unlink(path);
  unlink(SAVE_DIR "/run_history.sav.bak");
  CHECK(rhist_load() == RH_OK && rhist_count() == 0, "no file again: empty");
  CHECK(rhist_add(&e, &st) && st == RH_OK && rhist_count() == 1, "a new history starts");
  free(before);
  free(junk);

  /* ---- a failed write leaves the history as it was: the temp file is blocked by a folder */
  CHECK(rhist_load() == RH_OK && rhist_count() == 1, "one entry");
  {
    const rh_entry second = entry(8);
    CHECK(mkdir(SAVE_DIR "/run_history.sav.tmp", 0777) == 0, "block the temp file with a folder");
    CHECK(!rhist_add(&second, &st) && st == RH_OK, "a refused write is reported");
    CHECK(rhist_count() == 1 && rhist_get(0) && same(rhist_get(0), &e), "the history is as it was after a refused write");
    CHECK(rhist_load() == RH_OK && rhist_count() == 1 && same(rhist_get(0), &e), "and on disk");
    rmdir(SAVE_DIR "/run_history.sav.tmp");
    CHECK(rhist_add(&second, &st) && rhist_count() == 2, "it works again once the card does");
  }

  /* ---- what the slot list view of an entry holds */
  rsv_info in;
  const rh_entry v = entry(3);
  rhist_as_info(&v, &in);
  CHECK(in.status == RSV_OK && in.mode == v.mode && in.wave == v.wave && in.biome == v.biome && in.score == v.score && in.money == (double)v.money, "info fields");
  CHECK(in.n_party == v.n_party && !strcmp(in.name, v.name), "info party and name");
  for (int i = 0; i < in.n_party; i++)
    CHECK(in.party[i].species == v.party[i].species && in.party[i].level == (int32_t)v.party[i].level && in.party[i].fainted == v.party[i].fainted &&
              in.party[i].form == v.party[i].form && in.party[i].shiny == v.party[i].shiny,
          "info party %d", i);

  /* ---- an entry built from the end state of a run */
  static gc_run run;
  gc_run_new(&run, GAME_MODES_CLASSIC, "historytest000000000000001", 4321);
  gc_starter team[2];
  memset(team, 0, sizeof team);
  int n = 0, budget = gc_starter_budget(GAME_MODES_CLASSIC);
  for (int i = 0; i < PR_SPECIES_COUNT && n < 2; i++) {
    const pr_species* sp = &pr_species_table[i];
    const int cost = gc_starter_cost(sp->id);
    if (cost <= 0 || cost > budget) continue;
    team[n].species = (uint16_t)sp->id;
    team[n].tera_type = -1;
    for (int k = 0; k < 6; k++) team[n].ivs[k] = 15;
    budget -= cost;
    n++;
  }
  const char* why = NULL;
  CHECK(gc_run_set_starters(&run, team, n, &why), "starters: %s", why ? why : "?");
  gc_event ev;
  while (gc_run_next_event(&run, &ev)) {
  }
  run.wave = 41;
  run.score = 777;
  run.money = 1234.9;
  run.party[0].luck = 2;
  run.party[1].b.hp = 0;
  static rh_entry rec;
  /* a win: no defeat part */
  rhist_record(&run, true, 1760000123, "My run", &rec);
  CHECK(rec.victory && rec.defeat == RH_DEFEAT_NONE && rec.n_enemy == 0 && !rec.trainer[0], "a win names no enemy");
  CHECK(rec.time == 1760000123 && rec.wave == 41 && rec.score == 777 && rec.money == 1234 && rec.mode == run.run.mode && rec.biome == run.biome, "record fields");
  CHECK(rec.n_party == 2 && !strcmp(rec.name, "My run"), "record party and name");
  for (int i = 0; i < 2; i++) {
    const gc_mon* m = &run.party[run.party_order[i]];
    CHECK(rec.party[i].species == m->b.species && rec.party[i].level == (uint32_t)m->b.level && rec.party[i].nature == m->b.nature &&
              rec.party[i].ability == m->b.ability && rec.party[i].n_moves == m->b.n_moves && rec.party[i].stats[0] == (uint32_t)m->b.stats[0] &&
              rec.party[i].moves[0] == m->b.moves[0] && rec.party[i].fainted == (m->b.hp <= 0) && rec.party[i].hp == (m->b.hp > 0 ? (uint32_t)m->b.hp : 0),
          "record party %d", i);
  }
  CHECK(rec.party[0].luck + rec.party[1].luck == 2, "luck is kept");
  CHECK(rec.party[0].pp_max[0] > 0 && rec.party[0].types[0] >= 0, "pp and types are filled");
  rhist_record(&run, true, 0, NULL, &rec);
  CHECK(!rec.name[0] && rec.time == 0, "no name, no time");
  run.money = -5;
  rhist_record(&run, true, 1, NULL, &rec);
  CHECK(rec.money == 0, "negative money is 0");
  run.money = 1e300;
  rhist_record(&run, true, 1, NULL, &rec);
  CHECK(rec.money == (int64_t)9e15, "huge money is capped");

  /* a defeat by a wild double */
  run.battle_type = BATTLE_TYPE_WILD;
  run.trainer_type = -1;
  run.n_enemy = 2;
  run.enemy[0] = run.party[0];
  run.enemy[1] = run.party[1];
  run.enemy[0].b.level = 44;
  run.enemy[1].b.level = 45;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(!rec.victory && rec.defeat == RH_DEFEAT_WILD && rec.n_enemy == 2 && rec.enemy[0].level == 44 && rec.enemy[1].level == 45, "wild double");
  run.n_enemy = 5; /* a wild battle never has more, but a full trainer party left in the array must not overflow the entry */
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_WILD && rec.n_enemy == RH_ENEMIES, "at most two enemies are kept");
  run.n_enemy = 0;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_NONE && rec.n_enemy == 0, "a defeat with no enemy on record names none");
  /* a mystery encounter's wild battle is a wild battle */
  run.n_enemy = 1;
  run.battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_WILD && rec.n_enemy == 1, "a mystery encounter without a trainer");

  /* by a trainer, by the rival, by a pair */
  run.battle_type = BATTLE_TYPE_TRAINER;
  run.trainer_type = TRAINER_TYPE_ACE_TRAINER;
  run.trainer_variant = 0;
  run.trainer_names[GC_TNAME_TITLED] = "$t(trainerClasses:aceTrainer) $t(trainersCommon:aceTrainer.male.3)";
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_TRAINER && rec.n_enemy == 0 && !strcmp(rec.trainer, run.trainer_names[GC_TNAME_TITLED]) && !rec.trainer_double && !rec.trainer_female,
        "trainer: %s", rec.trainer);
  run.trainer_variant = TRAINER_VARIANT_DOUBLE;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_TRAINER && rec.trainer_double && !rec.trainer_female, "a pair");
  run.trainer_type = TRAINER_TYPE_RIVAL;
  run.trainer_variant = TRAINER_VARIANT_FEMALE;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_RIVAL && rec.trainer_female && !rec.trainer_double, "the female rival");
  run.trainer_type = TRAINER_TYPE_RIVAL_3;
  run.trainer_variant = 0;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_RIVAL, "a later rival");
  run.trainer_type = TRAINER_TYPE_ACE_TRAINER;
  run.battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_TRAINER, "a mystery encounter with a trainer");
  /* a name too long for the entry is cut by the caller's rule (no name has more than RSV_NAME_MAX characters); the entry still encodes */
  run.trainer_names[GC_TNAME_TITLED] = NULL;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(rec.defeat == RH_DEFEAT_TRAINER && !rec.trainer[0], "no trainer name from the engine");
  static char longname[400];
  memset(longname, 'z', sizeof longname - 1);
  run.trainer_names[GC_TNAME_TITLED] = longname;
  rhist_record(&run, false, 5, NULL, &rec);
  CHECK(!rec.trainer[0], "a name longer than the entry is left out, not cut in half");
  CHECK(rhist_encode(&rec, 1, g_buf, sizeof g_buf) > 0, "and the entry still encodes");

  printf("run_history_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
