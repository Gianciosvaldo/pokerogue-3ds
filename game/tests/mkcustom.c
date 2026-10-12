/*
 * mkcustom: writes the custom content file (save/custom.sav) with a few remembered enemies into ./sdmc:/3ds/pokerogue/save/, so the
 * Nemesis Radar (task C06) can be looked at on a real build (Azahar or a 3DS) without playing runs: the app itself never inserts a
 * Nemesis yet (docs/CUSTOM-CONTENT-CONTRACTS.md, section 2.10), so there is no other way to see a roster.
 *
 *   make mkcustom && (cd some/folder && /path/to/build/mkcustom few|full|empty|junk|future)
 *   then copy the `sdmc:` folder's 3ds/pokerogue/save/custom.sav into the SD card's 3ds/pokerogue/save/ and pick Nemesis Radar.
 *
 *   few:     nine records, newest first on the Radar: one with nothing known but the minimum, an unnamed biome, a shiny female, the
 *            longest species name, a cut-short loadout of eight items, a record held by a run, a beaten one and one whose reward was
 *            claimed.
 *   full:    sixteen records (the roster's limit), to scroll the list.
 *   empty:   a valid file with no records.
 *   junk:    a file the transport accepts but custom-core does not decode: the Radar says it cannot be used.
 *   future:  a valid file whose schema number is 2: the same.
 *
 * The file is made with custom-core and written with the app's own save.c; the tool decodes it again and fails when it does not
 * come back unchanged. Nothing here is part of the app.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cc/codec.h"
#include "cc/model.h"
#include "save.h"

static cc_meta M;
static uint8_t buf[CC_META_BUDGET_BYTES + 64];

#define SRC_RUN ((cc_run_uid)0x0000000700000002ull)
#define HOLD_RUN ((cc_run_uid)0x0000000700000003ull)

static cc_nemesis base(cc_record_id id, int species, int wave, int biome) {
  cc_nemesis n;
  memset(&n, 0, sizeof n);
  n.record_id = id;
  n.life = CC_LIFE_STORED;
  n.known = CC_NK_ABILITY | CC_NK_PASSIVE | CC_NK_MOVES | CC_NK_IVS | CC_NK_NATURE | CC_NK_LOADOUT | CC_NK_FINISH_MOVE | CC_NK_FALLEN |
            CC_NK_FINISHER_VERIFIED;
  n.source_run_uid = SRC_RUN;
  n.source_pokemon_id = 1000 + id;
  n.source_wave = (uint16_t)wave;
  n.source_biome = (uint8_t)biome;
  n.species = (uint16_t)species;
  n.gender = -1;
  n.orig_level = (uint8_t)(wave / 2 + 5);
  n.nature = (uint8_t)(id % 25);
  n.ability = 9;
  n.passive = 0;
  for (int i = 0; i < 6; i++) n.ivs[i] = (uint8_t)((id * 7 + i * 5) % 32);
  n.n_moves = 3;
  n.moves[0] = 33;
  n.moves[1] = 85;
  n.moves[2] = 53;
  n.n_items = 2;
  strcpy(n.items[0].key, "LEFTOVERS");
  n.items[0].arg = -1;
  n.items[0].stack = 1;
  strcpy(n.items[1].key, "FOCUS_BAND");
  n.items[1].arg = -1;
  n.items[1].stack = 2;
  n.finish_move = 85;
  n.fallen_species = 25;
  return n;
}

static void add(const cc_nemesis* n) { M.roster.rec[M.roster.count++] = *n; }

static void fresh(void) {
  memset(&M, 0, sizeof M);
  cc_meta_init(&M, 7);
  for (int i = 0; i < 40; i++) {
    cc_record_id r;
    cc_alloc_record_id(&M.counters, &r);
  }
  for (int i = 0; i < 8; i++) {
    cc_txn_id t;
    cc_alloc_txn_id(&M.counters, &t);
  }
  for (int i = 0; i < 12; i++) {
    cc_run_uid u;
    cc_alloc_run_uid(&M.counters, &u);
  }
}

static void few(void) {
  /* oldest first: the Radar lists the newest on top */
  cc_nemesis n = base(3, 94, 58, 19); /* Gengar, a graveyard, held by a run */
  n.life = CC_LIFE_SCHEDULED;
  n.sched_run_uid = HOLD_RUN;
  n.gender = 0;
  add(&n);
  n = base(5, 143, 82, 11); /* beaten in a rematch */
  n.life = CC_LIFE_DEFEATED;
  n.sched_run_uid = HOLD_RUN;
  n.challenge_wave = 77;
  add(&n);
  n = base(7, 9, 96, 6); /* reward claimed */
  n.life = CC_LIFE_REWARD_CLAIMED;
  n.sched_run_uid = HOLD_RUN;
  n.challenge_wave = 77;
  n.reward_txn = 4;
  add(&n);
  n = base(9, 150, 100, 25); /* a cut-short loadout of eight */
  n.known |= CC_NK_LOADOUT_TRUNCATED;
  n.n_items = CC_MAX_LOADOUT_ITEMS;
  {
    static const char* const keys[CC_MAX_LOADOUT_ITEMS] = {"LEFTOVERS", "SOUL_DEW", "FOCUS_BAND", "KINGS_ROCK", "QUICK_CLAW", "LUCKY_EGG", "SCOPE_LENS", "WIDE_LENS"};
    for (int i = 0; i < CC_MAX_LOADOUT_ITEMS; i++) {
      memset(&n.items[i], 0, sizeof n.items[i]);
      strcpy(n.items[i].key, keys[i]);
      n.items[i].arg = -1;
      n.items[i].stack = (uint16_t)(1 + i);
    }
  }
  add(&n);
  n = base(11, 8901, 121, 23); /* the longest species name */
  add(&n);
  n = base(13, 6, 27, 18); /* shiny, female */
  n.shiny = 1;
  n.gender = 1;
  add(&n);
  n = base(15, 133, 33, 50 /* End: not a biome the game names on this screen */);
  add(&n);
  n = base(17, 25, 8, 1); /* the minimum: nothing known but the ability and the moves */
  n.known = CC_NK_ABILITY | CC_NK_MOVES | CC_NK_FINISHER_VERIFIED;
  n.passive = 0;
  memset(n.ivs, 0, sizeof n.ivs);
  n.nature = 0;
  n.n_items = 0;
  memset(n.items, 0, sizeof n.items);
  n.finish_move = 0;
  n.fallen_species = 0;
  n.n_moves = 1;
  n.moves[1] = n.moves[2] = 0;
  add(&n);
  n = base(19, 149, 140, 4); /* Dragonite, a plain stored record on top */
  n.gender = 0;
  add(&n);
}

static void full(void) {
  static const int species[] = {25, 6, 94, 143, 9, 150, 133, 149, 130, 65, 68, 3, 112, 248, 445, 700};
  static const int biome[] = {1, 18, 19, 11, 6, 25, 5, 4, 8, 13, 20, 14, 12, 26, 27, 31};
  for (int i = 0; i < CC_MAX_NEMESES; i++) {
    cc_nemesis n = base((cc_record_id)(3 + 2 * i), species[i], 10 + i * 9, biome[i]);
    n.gender = (int8_t)(i % 3 - 1);
    n.shiny = i % 5 == 0;
    add(&n);
  }
}

int main(int argc, char** argv) {
  const char* what = argc > 1 ? argv[1] : "few";
  fresh();
  if (!strcmp(what, "few"))
    few();
  else if (!strcmp(what, "full"))
    full();
  else if (strcmp(what, "empty") && strcmp(what, "junk") && strcmp(what, "future")) {
    fprintf(stderr, "usage: mkcustom few|full|empty|junk|future\n");
    return 2;
  }
  size_t n = 0;
  const cc_status vs = cc_meta_validate(&M);
  if (vs != CC_OK) {
    fprintf(stderr, "the fixture does not validate (status %d)\n", (int)vs);
    return 1;
  }
  if (cc_meta_encode(&M, buf, sizeof buf, &n) != CC_OK) {
    fprintf(stderr, "could not encode\n");
    return 1;
  }
  if (!strcmp(what, "junk")) {
    memset(buf, 0x5a, 200);
    n = 200;
  } else if (!strcmp(what, "future")) {
    buf[4] = 2; /* schema, little-endian, after the 4-byte magic */
    buf[5] = 0;
  } else {
    static cc_meta back;
    static uint8_t again[CC_META_BUDGET_BYTES + 64];
    size_t n2 = 0;
    if (cc_meta_decode(buf, n, &back) != CC_OK || cc_meta_encode(&back, again, sizeof again, &n2) != CC_OK || n2 != n || memcmp(again, buf, n) != 0) {
      fprintf(stderr, "the file does not decode back unchanged\n");
      return 1;
    }
  }
  mkdir("sdmc:", 0777); /* save_init makes only the last folder */
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/pokerogue", 0777);
  if (!save_init()) {
    fprintf(stderr, "could not create the save folder\n");
    return 1;
  }
  if (save_write("custom", buf, n) != SAVE_OK) {
    fprintf(stderr, "could not write custom.sav\n");
    return 1;
  }
  printf("wrote sdmc:/3ds/pokerogue/save/custom.sav (%s, %zu bytes, %d records)\n", what, n, (int)M.roster.count);
  return 0;
}
