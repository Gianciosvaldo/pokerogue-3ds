/*
 * PokeRogue 3DS: the game app.
 *
 * For now it boots straight into a demo battle: Blaziken against a wild Pokemon, each turn run by
 * battle-core and shown from its events (battle_events.c). The battle is built by a stand-in
 * (battle_setup.c) until the app drives game-core's run; the enemy picks random moves until
 * ai-core. RUN always gets away (fleeing isn't in battle-core yet) and a new wild Pokemon appears;
 * BALL and POKEMON say they aren't wired up yet.
 *
 * START + SELECT quits.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_events.h"
#include "battle_setup.h"
#include "battle_view.h"
#include "i18n.h"
#include "pak.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "rr.h"
#include "save.h"
#include "snd.h"
#include "ui.h"
#include "ui_widgets.h"

/* ------------------------------------------------------------------ demo */

static const int WILD[] = {SPECIES_ID_GARCHOMP, SPECIES_ID_GARDEVOIR, SPECIES_ID_LUCARIO,
                           SPECIES_ID_GYARADOS, SPECIES_ID_PIKACHU,   SPECIES_ID_MIMIKYU};
static const int BIOMES[] = {BIOME_ID_GRASS, BIOME_ID_TALL_GRASS, BIOME_ID_CAVE, BIOME_ID_BEACH, BIOME_ID_FOREST};

static struct {
  bs_battle b;
  int n;       /* battles so far */
  int level;   /* the player's level, kept between battles */
} D;

static void new_battle(void* arg);
static void ask(void* arg);

/* The species' last four level-up moves at or below `level` (a stand-in for run-core's moveset). */
static int level_moves(int species, int level, int out[4]) {
  const pr_species* sp = NULL;
  for (int i = 0; i < PR_SPECIES_COUNT; i++)
    if (pr_species_table[i].id == species) sp = &pr_species_table[i];
  if (!sp) return 0;
  int n = 0;
  for (int i = 0; i < sp->level_moves.n; i++) {
    const pr_level_move* lm = &sp->level_moves.v[i];
    if (lm->level > level || lm->level < 1) continue;
    bool dup = false;
    for (int k = 0; k < n; k++) dup |= out[k] == lm->move;
    if (dup) continue;
    if (n == 4) memmove(out, out + 1, 3 * sizeof *out), n = 3;
    out[n++] = lm->move;
  }
  return n;
}

static void add_msg(bv_event* ev, int* n, const char* text, bool prompt) {
  ev[(*n)++] = (bv_event){.type = BEV_MESSAGE, .text = text, .prompt = prompt};
}

static void on_command(bv_command c, void* arg) {
  (void)arg;
  static bv_event ev[192];
  static char text[160];
  int n = 0;
  if (c.command == 3) {
    /* fleeing isn't in battle-core yet: always succeeds here */
    ui_play_sound("se/flee");
    i18n_t(text, sizeof text, "battle:runAwaySuccess", NULL);
    add_msg(ev, &n, text, true);
    bv_play(ev, n, new_battle, NULL);
    return;
  }
  if (c.command != 0) {
    snprintf(text, sizeof text, "The %s screen isn't in yet.", c.command == 1 ? "ball" : "party");
    add_msg(ev, &n, text, true);
    bv_play(ev, n, ask, NULL);
    return;
  }
  /* a real battle-core turn: the enemy picks a random move until ai-core is wired in */
  bv_mon before_p = *bv_mon_of(BV_PLAYER), before_e = *bv_mon_of(BV_ENEMY);
  bs_turn_fight(&D.b, c.move_slot);
  n = be_convert(&D.b.bt, &D.b.eng, &before_p, &before_e, ev, 190);
  /* the fight menu reads PP from the screen's copy */
  bv_mon* shown = bv_mon_of(BV_PLAYER);
  for (int i = 0; i < 4; i++) {
    const pr_pokemon* p = D.b.bt.field[0];
    int max = p->max_pp_override[i] >= 0 ? p->max_pp_override[i] : pr_moves[p->moves[i]].pp;
    shown->pp_left[i] = max - p->pp_used[i];
  }
  int out = bs_outcome(&D.b);
  if (out == 2) D.level = 0; /* lost: start over */
  bv_play(ev, n, out ? new_battle : ask, NULL);
}

static void ask(void* arg) {
  (void)arg;
  bv_ask_command(on_command, NULL);
}

static void start_battle(void* arg) {
  (void)arg;
  if (!D.level) D.level = 50;
  int wild = WILD[D.n % (int)(sizeof WILD / sizeof WILD[0])];
  int biome = BIOMES[D.n % (int)(sizeof BIOMES / sizeof BIOMES[0])];
  D.n++;
  char seed[32];
  snprintf(seed, sizeof seed, "demo%lu", (unsigned long)(osGetTime() & 0xffffff));
  bs_begin(&D.b, biome, D.n, seed);
  static const int PLAYER_MOVES[] = {MOVE_ID_BLAZE_KICK, MOVE_ID_CLOSE_COMBAT, MOVE_ID_BRAVE_BIRD, MOVE_ID_SWORDS_DANCE};
  bs_add(&D.b, 0, SPECIES_ID_BLAZIKEN, D.level, PLAYER_MOVES, 4);
  int em[4];
  int enl = 45 + rand() % 10;
  int nm = level_moves(wild, enl, em);
  bs_add(&D.b, 1, wild, enl, em, nm);

  bv_setup s = {0};
  s.biome = biome;
  be_mon(&D.b.bt, D.b.bt.field[0], &s.player);
  be_mon(&D.b.bt, D.b.bt.field[2], &s.enemy);
  s.player.exp_ratio = 0.3f;
  s.enemy.caught_before = rand() % 2;
  bv_begin(&s);
  bv_event ev[] = {
      {.type = BEV_INTRO_WILD, .battler = BV_ENEMY},
      {.type = BEV_SUMMON, .battler = BV_PLAYER, .mon = &s.player},
  };
  bv_play(ev, 2, ask, NULL);
}

static void new_battle(void* arg) {
  (void)arg;
  img_drop_prefix("images/pokemon/");
  img_drop_prefix("images/arenas/");
  ui_fade(250, 100, 250, start_battle, NULL);
}

/* ------------------------------------------------------------------ boot */

static void fatal(const char* msg) {
  /* no packs: say so on the bottom screen until START */
  while (aptMainLoop()) {
    hidScanInput();
    if (hidKeysDown() & KEY_START) break;
    rr_frame_begin();
    rr_target(RR_TOP, RR_RGBA(0, 0, 0, 255));
    rr_target(RR_BOTTOM, RR_RGBA(0x20, 0, 0, 255));
    rr_text_sys(8, 8, 0.5f, msg, RR_RGBA(0xff, 0xff, 0xff, 255));
    rr_frame_end();
  }
}

int main(void) {
  osSetSpeedupEnable(true); /* no-op on Old 3DS */
  if (!rr_init()) return 1;
  if (pak_mount_default() <= 0) {
    fatal("PokeRogue 3DS\n\nNo asset packs found in\nsdmc:/3ds/pokerogue/\n\nSee assets-pipeline/README.md\nfor how to build them.\n\nSTART: quit");
    rr_exit();
    return 0;
  }
  snd_init();
  save_init();
  srand((unsigned)osGetTime());
  ui_init();
  start_battle(NULL);
  while (ui_tick()) {
  }
  snd_exit();
  rr_exit();
  return 0;
}
