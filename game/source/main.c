/*
 * PokeRogue 3DS: the game app.
 *
 * For now it boots straight into a demo battle: the battle screen playing back a scripted event
 * stream (Blaziken against a wild Pokemon), until battle-core publishes its event header and the
 * run loop drives the screen for real. FIGHT picks a move and plays a scripted turn with rough
 * damage numbers (not battle-core's: they are only there to move the HP bars); RUN leaves and a
 * new wild Pokemon appears; BALL and POKEMON say they aren't wired up yet.
 *
 * START + SELECT quits.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ------------------------------------------------------------------ demo data */

static const pr_species* species(int id) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == id) return &pr_species_table[mid];
    if (pr_species_table[mid].id < id) lo = mid + 1;
    else hi = mid - 1;
  }
  return NULL;
}

static void make_mon(bv_mon* m, int id, int level, const int* moves, int n_moves, int max_hp) {
  memset(m, 0, sizeof *m);
  m->species = id;
  m->level = level;
  m->hp = m->max_hp = max_hp;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(id));
  const pr_species* sp = species(id);
  m->gender = !sp || sp->male_permille < 0 ? -1 : rand() % 1000 < sp->male_permille ? 0 : 1;
  m->female = m->gender == 1;
  if (sp) {
    const pr_form* f = &pr_forms[sp->form0];
    m->types[m->n_types++] = f->type1;
    if (f->type2 >= 0) m->types[m->n_types++] = f->type2;
  }
  for (int i = 0; i < n_moves && i < 4; i++) {
    m->moves[i] = moves[i];
    m->pp_left[i] = m->pp_max[i] = pr_moves[moves[i]].pp;
  }
  m->n_moves = n_moves;
  m->exp_ratio = 0.3f;
}

static const int WILD[] = {SPECIES_ID_GARCHOMP, SPECIES_ID_GARDEVOIR, SPECIES_ID_LUCARIO,
                           SPECIES_ID_GYARADOS, SPECIES_ID_PIKACHU,   SPECIES_ID_MIMIKYU};
static const int BIOMES[] = {BIOME_ID_GRASS, BIOME_ID_TALL_GRASS, BIOME_ID_CAVE, BIOME_ID_BEACH, BIOME_ID_FOREST};

static struct {
  bv_mon player, enemy;
  int wild;
} D;

/* ------------------------------------------------------------------ demo flow */

static void new_battle(void* arg);
static void ask(void* arg);

static void add_msg(bv_event* ev, int* n, const char* text, int delay, bool prompt) {
  ev[*n] = (bv_event){.type = BEV_MESSAGE, .text = text, .a = delay, .prompt = prompt};
  (*n)++;
}

static int type_mult_x2(int move_type, const bv_mon* def) {
  /* pr_type_chart_x2 holds the multiplier x2 (0, 1, 2, 4); combine both types */
  int m = 4;
  for (int i = 0; i < def->n_types; i++) m = m * pr_type_chart_x2[move_type][def->types[i]] / 2;
  return m; /* x4: 4 = neutral */
}

/* One scripted attack: message, animation, damage, effectiveness, faint. Returns true on a faint. */
static bool attack(bv_event* ev, int* n, int user, int slot, char texts[][160], int* nt) {
  bv_mon* a = user == BV_PLAYER ? &D.player : &D.enemy;
  bv_mon* d = user == BV_PLAYER ? &D.enemy : &D.player;
  int target = user == BV_PLAYER ? BV_ENEMY : BV_PLAYER;
  int move = a->moves[slot];
  const pr_move* mv = &pr_moves[move];
  if (a->pp_left[slot] > 0) a->pp_left[slot]--;

  char wild[96];
  if (user == BV_PLAYER) snprintf(wild, sizeof wild, "%s", a->name);
  else i18n_t(wild, sizeof wild, "battle:wildPokemonWithAffix", "pokemonName", a->name, NULL);
  i18n_t(texts[*nt], 160, "battle:useMove", "pokemonNameWithAffix", wild, "moveName", i18n_move_name(move), NULL);
  add_msg(ev, n, texts[(*nt)++], 500, false);
  ev[(*n)++] = (bv_event){.type = BEV_MOVE_ANIM, .battler = user, .target = target, .a = move};

  if (mv->power <= 0) {
    /* a status move: raise the user's attack by 2, like Swords Dance */
    ev[(*n)++] = (bv_event){.type = BEV_STAT_STAGES, .battler = user, .a = STAT_ATK, .b = 2};
    return false;
  }
  int eff = type_mult_x2(mv->type, d);
  if (eff == 0) {
    i18n_t(texts[*nt], 160, "battle:hitResultNoEffect", "pokemonName", d->name, NULL);
    add_msg(ev, n, texts[(*nt)++], 0, false);
    return false;
  }
  int dmg = d->max_hp * mv->power / 300 * eff / 4;
  dmg = dmg * (85 + rand() % 16) / 100;
  if (dmg < 1) dmg = 1;
  d->hp = d->hp > dmg ? d->hp - dmg : 0;
  /* HitResult: 1 EFFECTIVE, 2 SUPER_EFFECTIVE, 3 NOT_VERY_EFFECTIVE */
  ev[(*n)++] = (bv_event){.type = BEV_DAMAGE, .battler = target, .a = d->hp, .b = eff > 4 ? 2 : eff < 4 ? 3 : 1};
  if (eff != 4) {
    i18n_t(texts[*nt], 160, eff > 4 ? "battle:hitResultSuperEffective" : "battle:hitResultNotVeryEffective", NULL);
    add_msg(ev, n, texts[(*nt)++], 0, false);
  }
  if (d->hp == 0) {
    ev[(*n)++] = (bv_event){.type = BEV_FAINT, .battler = target};
    char nm[96];
    if (target == BV_PLAYER) snprintf(nm, sizeof nm, "%s", d->name);
    else i18n_t(nm, sizeof nm, "battle:wildPokemonWithAffix", "pokemonName", d->name, NULL);
    i18n_t(texts[*nt], 160, "battle:fainted", "pokemonNameWithAffix", nm, NULL);
    add_msg(ev, n, texts[(*nt)++], 0, true);
    return true;
  }
  return false;
}

static void on_command(bv_command c, void* arg) {
  (void)arg;
  static bv_event ev[24];
  static char texts[12][160];
  int n = 0, nt = 0;
  if (c.command == 3) {
    ui_play_sound("se/flee");
    i18n_t(texts[nt], 160, "battle:runAwaySuccess", NULL);
    add_msg(ev, &n, texts[nt++], 0, true);
    bv_play(ev, n, new_battle, NULL);
    return;
  }
  if (c.command != 0) {
    snprintf(texts[nt], 160, "The %s screen isn't in yet.", c.command == 1 ? "ball" : "party");
    add_msg(ev, &n, texts[nt++], 0, true);
    bv_play(ev, n, ask, NULL);
    return;
  }
  bool enemy_fainted = attack(ev, &n, BV_PLAYER, c.move_slot, texts, &nt);
  bool player_fainted = false;
  if (!enemy_fainted) player_fainted = attack(ev, &n, BV_ENEMY, rand() % D.enemy.n_moves, texts, &nt);
  if (enemy_fainted) {
    int exp = 120 + rand() % 400;
    char amount[16];
    snprintf(amount, sizeof amount, "%d", exp);
    i18n_t(texts[nt], 160, "battle:expGain", "pokemonName", D.player.name, "exp", amount, NULL);
    add_msg(ev, &n, texts[nt++], 0, true);
    float to = D.player.exp_ratio + exp / 1000.0f;
    int new_level = 0;
    if (to >= 1) {
      to -= 1;
      new_level = ++D.player.level;
    }
    D.player.exp_ratio = to;
    ev[n++] = (bv_event){.type = BEV_EXP, .battler = BV_PLAYER, .a = (int)(to * 1000), .b = new_level};
    if (new_level) {
      char lv[16];
      snprintf(lv, sizeof lv, "%d", new_level);
      i18n_t(texts[nt], 160, "battle:levelUp", "pokemonName", D.player.name, "level", lv, NULL);
      add_msg(ev, &n, texts[nt++], 0, true);
    }
  }
  bv_play(ev, n, enemy_fainted || player_fainted ? new_battle : ask, NULL);
}

static void ask(void* arg) {
  (void)arg;
  bv_ask_command(on_command, NULL);
}

static void start_battle(void* arg) {
  (void)arg;
  static const int PLAYER_MOVES[] = {MOVE_ID_BLAZE_KICK, MOVE_ID_CLOSE_COMBAT, MOVE_ID_BRAVE_BIRD, MOVE_ID_SWORDS_DANCE};
  static const int ENEMY_MOVES[] = {MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_TACKLE};
  int level = D.player.level ? D.player.level : 50;
  float exp = D.player.level ? D.player.exp_ratio : 0.3f;
  make_mon(&D.player, SPECIES_ID_BLAZIKEN, level, PLAYER_MOVES, 4, 150 + level * 3);
  D.player.exp_ratio = exp;
  int wild = WILD[D.wild++ % (int)(sizeof WILD / sizeof WILD[0])];
  make_mon(&D.enemy, wild, 45 + rand() % 10, ENEMY_MOVES, 3, 140 + rand() % 60);
  D.enemy.caught_before = rand() % 2;

  bv_setup s = {0};
  s.biome = BIOMES[(D.wild - 1) % (int)(sizeof BIOMES / sizeof BIOMES[0])];
  s.player = D.player;
  s.enemy = D.enemy;
  bv_begin(&s);
  bv_event ev[] = {
      {.type = BEV_INTRO_WILD, .battler = BV_ENEMY},
      {.type = BEV_SUMMON, .battler = BV_PLAYER, .mon = &D.player},
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
