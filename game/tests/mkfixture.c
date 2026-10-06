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

static bool begin(const char* seed, int species, int wave, int biome) {
  gc_run_new(&R, GAME_MODES_CLASSIC, seed, 5);
  if (wave > 1) {
    R.wave = wave - 1; /* the first encounter is the next wave */
    R.biome = biome;
  }
  gc_starter team[1];
  memset(team, 0, sizeof team);
  team[0].species = (uint16_t)species;
  team[0].tera_type = -1;
  for (int k = 0; k < 6; k++) team[0].ivs[k] = 31;
  const char* why = NULL;
  if (!gc_run_set_starters(&R, team, 1, &why)) {
    printf("no starters: %s\n", why ? why : "?");
    return false;
  }
  drain(&R);
  return gc_run_screen(&R)->kind == GC_SCREEN_COMMAND && gc_run_can_save(&R);
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

int main(int argc, char** argv) {
  if (argc != 2 || (strcmp(argv[1], "gameover") && strcmp(argv[1], "clear") && strcmp(argv[1], "reward"))) {
    printf("usage: mkfixture gameover|clear|reward   (writes ./sdmc:/3ds/pokerogue/save/session_0.sav)\n");
    return 2;
  }
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
