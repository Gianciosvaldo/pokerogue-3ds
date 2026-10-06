/*
 * mkfixture: writes a run save into slot 0 of ./sdmc:/3ds/pokerogue/save/ that is one turn away from an ending (or from the
 * first rewards), so the game over, clear and reward screens can be looked at on a real build (Azahar or a 3DS) in seconds
 * instead of playing 200 waves.
 *
 *   make mkfixture && (cd some/folder && /path/to/build/mkfixture gameover|clear|reward)
 *   then copy the `sdmc:` folder's 3ds/pokerogue/save into the SD card's 3ds/pokerogue/ and pick Continue.
 *
 *   gameover: wave 1, the only Pokemon has 1 HP and the wild one is faster and hits hard: the first turn ends the run.
 *   clear:    wave 200 (the end biome), Eternatus at 1 HP and speed 1: the first turn wins the run.
 *   reward:   wave 1, the wild Pokemon has 1 HP and speed 1: the first turn wins the battle and the rewards come up.
 *   catch:    wave 1, a Master Ball in the bag: Ball, then the Master Ball, is a catch (3 shakes, "Gotcha!").
 *   breakfree: wave 1, five Poke Balls: Ball, then the Poke Ball, is a throw that breaks free (a seed is searched for it).
 *   fullparty: wave 1, six in the party and a Master Ball: Ball, then the Master Ball, catches and asks which member to release.
 *
 * The fixtures are made with the real game-core and written with the app's own run_saves.c, then loaded back and played
 * to their ending with the run's own rules (never the screens): the tool fails when a fixture does not end the way its
 * name says. Nothing here is part of the app.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "gc/run.h"
#include "pr_enums.h"
#include "run_saves.h"
#include "save.h"

static gc_run R;

static void drain(gc_run* r) {
  gc_event e;
  while (gc_run_next_event(r, &e)) {
  }
}

static bool begin_team(const char* seed, const int* species, int n, int wave, int biome) {
  gc_run_new(&R, GAME_MODES_CLASSIC, seed, 5);
  if (wave > 1) {
    R.wave = wave - 1; /* the first encounter is the next wave */
    R.biome = biome;
  }
  gc_starter team[6];
  memset(team, 0, sizeof team);
  for (int i = 0; i < n; i++) {
    team[i].species = (uint16_t)species[i];
    team[i].tera_type = -1;
    for (int k = 0; k < 6; k++) team[i].ivs[k] = 31;
  }
  const char* why = NULL;
  if (!gc_run_set_starters(&R, team, n, &why)) {
    printf("no starters: %s\n", why ? why : "?");
    return false;
  }
  drain(&R);
  return gc_run_screen(&R)->kind == GC_SCREEN_COMMAND && gc_run_can_save(&R);
}

static bool begin(const char* seed, int species, int wave, int biome) { return begin_team(seed, &species, 1, wave, biome); }

/* What a throw of `ball` at the loaded fixture does: 1 it held (the party grew, or the full-party question came up), 0 it broke free,
 * -1 something else. The same answers the app gives (Ball, then the ball), so the app plays the fixture the same way. */
static int throw_outcome(gc_run* r, int ball, bool* asks) {
  gc_choice c;
  memset(&c, 0, sizeof c);
  c.kind = GC_CH_BALL;
  c.a = (int16_t)ball;
  c.b = c.c = -1;
  const int before = r->n_party;
  const char* why = NULL;
  if (!gc_run_choose(r, &c, &why)) return -1;
  drain(r);
  const gc_screen* s = gc_run_screen(r);
  *asks = s->kind == GC_SCREEN_SWITCH && s->can_skip && before == 6;
  if (*asks || r->n_party > before) return 1;
  return r->enemy[0].b.hp > 0 && s->kind == GC_SCREEN_COMMAND ? 0 : -1;
}

/* Plays a loaded fixture with the first move each turn and answers what the run asks, up to its ending. */
static int play_to_end(gc_run* r) {
  for (int t = 0; t < 40; t++) {
    const gc_screen* s = gc_run_screen(r);
    gc_choice c;
    memset(&c, 0, sizeof c);
    c.a = -1;
    c.b = -1;
    c.c = -1;
    switch (s->kind) {
      case GC_SCREEN_GAME_OVER:
      case GC_SCREEN_CLEAR:
      case GC_SCREEN_REWARD:
        return s->kind;
      case GC_SCREEN_COMMAND:
        c.kind = GC_CH_FIGHT;
        c.a = 0;
        break;
      case GC_SCREEN_LEARN_MOVE:
        c.kind = GC_CH_FORGET;
        break;
      case GC_SCREEN_EVOLVE:
        c.kind = GC_CH_CANCEL;
        break;
      case GC_SCREEN_SWITCH:
        c.kind = GC_CH_PICK;
        for (int i = 0; i < 6; i++)
          if (s->pick_ok[i]) {
            c.a = (int16_t)i;
            break;
          }
        break;
      default:
        return -(int)s->kind - 1; /* something this tool does not answer */
    }
    const char* why = NULL;
    if (!gc_run_choose(r, &c, &why)) return -100;
    drain(r);
  }
  return -101;
}


/* catch, breakfree, fullparty: a wild battle at wave 1 with balls in the bag. The seed is searched until the throw of the ball the name
 * promises ends as promised (replaying what the app will do from the saved slot), so the fixture can be played to that screen. */
static int make_catch_fixture(const char* name) {
  const bool catch_ = !strcmp(name, "catch"), full = !strcmp(name, "fullparty");
  static const int six[6] = {SPECIES_ID_CATERPIE, SPECIES_ID_WEEDLE, SPECIES_ID_PIDGEY, SPECIES_ID_RATTATA, SPECIES_ID_SPEAROW,
                             SPECIES_ID_ZUBAT};
  const int ball = catch_ || full ? POKEBALL_TYPE_MASTER_BALL : POKEBALL_TYPE_POKEBALL;
  mkdir("sdmc:", 0777);
  if (!save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  for (int attempt = 0; attempt < 400; attempt++) {
    char seed[40];
    snprintf(seed, sizeof seed, "fixture%s%014d", name, attempt);
    const int one = SPECIES_ID_MAGIKARP;
    if (!(full ? begin_team(seed, six, 6, 1, BIOME_ID_PLAINS) : begin(seed, one, 1, BIOME_ID_PLAINS))) continue;
    if (full && R.n_party != 6) continue;
    R.pokeballs[ball] = catch_ || full ? 1 : 5;
    if (rsv_save(0, &R) != RSV_SAVED) {
      printf("FAILED: could not save\n");
      return 1;
    }
    static gc_run L;
    char why[RSV_WHY_LEN];
    if (rsv_load(0, &L, why) != RSV_OK) {
      printf("FAILED: the slot does not load back: %s\n", why);
      return 1;
    }
    bool asks = false;
    const int sp = L.enemy[0].b.species, hp = L.enemy[0].b.hp, max_hp = L.enemy[0].b.stats[STAT_HP];
    const int out = throw_outcome(&L, ball, &asks);
    const bool good = catch_ ? out == 1 && !asks : full ? out == 1 && asks : out == 0;
    if (!good) continue;
    /* the slot holds the good attempt (the last save); say what the player will see */
    printf("%s fixture: seed %s, wave %d, wild species %d at HP %d/%d, %d in the party, a %s Ball is thrown: %s%s\n", name, seed, L.wave, sp,
           hp, max_hp, full ? 6 : 1, ball == POKEBALL_TYPE_MASTER_BALL ? "Master" : "Poke", out == 1 ? "it holds" : "it breaks free",
           asks ? ", the release question comes up" : "");
    return 0;
  }
  printf("FAILED: no seed in 400 makes the %s throw end as promised\n", name);
  return 1;
}

int main(int argc, char** argv) {
  if (argc != 2 || (strcmp(argv[1], "gameover") && strcmp(argv[1], "clear") && strcmp(argv[1], "reward") && strcmp(argv[1], "catch") &&
                    strcmp(argv[1], "breakfree") && strcmp(argv[1], "fullparty"))) {
    printf("usage: mkfixture gameover|clear|reward|catch|breakfree|fullparty   (writes ./sdmc:/3ds/pokerogue/save/session_0.sav)\n");
    return 2;
  }
  if (!strcmp(argv[1], "catch") || !strcmp(argv[1], "breakfree") || !strcmp(argv[1], "fullparty")) return make_catch_fixture(argv[1]);
  const bool clear = !strcmp(argv[1], "clear");
  const bool reward = !strcmp(argv[1], "reward");
  mkdir("sdmc:", 0777);
  if (!save_init()) {
    printf("FAILED: no folder for the saves\n");
    return 1;
  }
  bool ok;
  if (clear) {
    ok = begin("fixtureclear00000000000001", SPECIES_ID_CHARMANDER, 200, BIOME_ID_END);
    if (ok) {
      /* level 100 so the knockout gives no level-up: nothing but the ending to look at */
      R.party[0].b.level = 100;
      R.enemy[0].b.hp = 1;
      R.enemy[0].b.stats[STAT_SPD] = 1;
    }
  } else if (reward) {
    ok = begin("fixturereward0000000000001", SPECIES_ID_CHARMANDER, 1, BIOME_ID_PLAINS);
    if (ok) {
      R.enemy[0].b.hp = 1;
      R.enemy[0].b.stats[STAT_SPD] = 1;
    }
  } else {
    ok = begin("fixturegameover000000000001", SPECIES_ID_MAGIKARP, 1, BIOME_ID_PLAINS);
    if (ok) {
      R.party[0].b.hp = 1;
      R.enemy[0].b.moves[0] = MOVE_ID_TACKLE;
      R.enemy[0].b.n_moves = 1;
      R.enemy[0].b.stats[STAT_ATK] = 999;
      R.enemy[0].b.stats[STAT_SPD] = 999;
    }
  }
  if (!ok) {
    printf("FAILED: the run is not at a save point\n");
    return 1;
  }
  if (rsv_save(0, &R) != RSV_SAVED) {
    printf("FAILED: could not save\n");
    return 1;
  }
  /* the saved slot, loaded back, must end as promised */
  static gc_run L;
  char why[RSV_WHY_LEN];
  if (rsv_load(0, &L, why) != RSV_OK) {
    printf("FAILED: the slot does not load back: %s\n", why);
    return 1;
  }
  const int end = play_to_end(&L);
  const int want = clear ? GC_SCREEN_CLEAR : reward ? GC_SCREEN_REWARD : GC_SCREEN_GAME_OVER;
  const char* const names[] = {"game over", "clear", "reward"};
  const int got_i = end == GC_SCREEN_GAME_OVER ? 0 : end == GC_SCREEN_CLEAR ? 1 : end == GC_SCREEN_REWARD ? 2 : -1;
  printf("%s fixture: wave %d, loaded back and played to the %s screen (wanted the %s one): %s\n", argv[1], L.wave,
         got_i < 0 ? "other" : names[got_i], names[clear ? 1 : reward ? 2 : 0], end == want ? "OK" : "FAILED");
  return end == want ? 0 : 1;
}
