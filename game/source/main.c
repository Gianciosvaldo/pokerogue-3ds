/*
 * PokeRogue 3DS: the game app.
 *
 * Boots into the title (app_flow.c: Continue, New Game and Load Game with five save slots). A run is game-core's run
 * loop; run_screen.c shows it with the battle screen (battle_events.c turns the turns' events into screen events), the
 * party and summary screens (game/menus), the reward and biome screens, the Poke Ball list and option lists, and saves it
 * at every save point. See run_screen.h for what is still a stand-in (starters, reward names, mystery encounters...).
 *
 * Debug file: sdmc:/3ds/pokerogue/rundriver.txt, optional. Lines "seed=<24 characters>" and
 * "starters=<species id>,<species id>..." fix the first New Game of a session (for repeatable test runs). A line "capsule=<1..25>" opens
 * the capsule-machine preview (a debug view with fixed results, capsule_screen.h) before the title.
 *
 * START + SELECT quits.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_flow.h"
#include "i18n.h"
#include "mui.h"
#include "pak.h"
#include "rr.h"
#include "run_screen.h"
#include "save.h"
#include "snd.h"
#include "ui.h"
#include "ui_widgets.h"
#include "party_screen.h"
#include "runtime_limits.h"

/* Strong override of libctru's weak startup default, used by 3DSX and CIA. */
u32 __stacksize__ = POKEROGUE_MAIN_STACK_BYTES;

/* ------------------------------------------------------------------ boot */

/* sdmc:/3ds/pokerogue/rundriver.txt (see the header comment) */
static void read_driver_file(rs_config* cfg) {
  app_debug_capsule(0);
  static char seed[160];
  FILE* f = fopen("sdmc:/3ds/pokerogue/rundriver.txt", "r");
  if (!f) return;
  char line[160];
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "seed=", 5) && line[5]) {
      snprintf(seed, sizeof seed, "%s", line + 5);
      cfg->seed = seed;
    } else if (!strncmp(line, "capsule=", 8)) {
      app_debug_capsule(atoi(line + 8)); /* anything outside 1..25 is ignored there */
    } else if (!strncmp(line, "touch=", 6)) { /* "touch=<frame>:<x>,<y>": a tap at that frame (emulator runs have no touch) */
      unsigned fr = 0;
      int tx = 0, ty = 0;
      if (sscanf(line + 6, "%u:%d,%d", &fr, &tx, &ty) == 3) ui_debug_touch(fr, (float)tx, (float)ty);
    } else if (!strncmp(line, "starters=", 9)) {
      cfg->n_starters = 0;
      for (char* t = strtok(line + 9, ", "); t && cfg->n_starters < 6; t = strtok(NULL, ", "))
        cfg->starters[cfg->n_starters++] = atoi(t);
    }
  }
  fclose(f);
}

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
    fatal("PokeRogue 3DS\n\nNo asset packs found, inside\nthe app or in sdmc:/3ds/pokerogue/\n\nSee sd-builder/README.md\nfor how to build them.\n\nSTART: quit");
    rr_exit();
    return 0;
  }
  snd_init();
  save_init();
  srand((unsigned)osGetTime());
  ui_init();
  mui_set_window_style(ui_window_style());
  ps_init();
  rs_config cfg;
  memset(&cfg, 0, sizeof cfg);
  cfg.entropy = (uint32_t)osGetTime();
  read_driver_file(&cfg);
  app_start(&cfg);
  while (ui_tick()) {
  }
  snd_exit();
  rr_exit();
  return 0;
}
