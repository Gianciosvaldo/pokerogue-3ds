/*
 * PokeRogue 3DS: main entry point and driver loop.
 *
 * Boots into Title UI or Save Slots UI, allows starting a new run or continuing an existing run,
 * saves progress inter-wave to SD card, and displays Game Over / Clear victory screens.
 */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clear_ui.h"
#include "game_over_ui.h"
#include "gc/system.h"
#include "i18n.h"
#include "pak.h"
#include "party_screen.h"
#include "rr.h"
#include "run_screen.h"
#include "save.h"
#include "save_slot_ui.h"
#include "snd.h"
#include "title_ui.h"
#include "ui.h"

static gc_system sys_data;
static rs_config cfg;
static int current_slot = 0;
static int title_continue_slot = 0;
static bool slot_0_occupied = false;

static void read_driver_file(rs_config* cfg_out) {
  static char seed[160];
  FILE* f = fopen("sdmc:/3ds/pokerogue/rundriver.txt", "r");
  if (!f) return;
  char line[160];
  while (fgets(line, sizeof line, f)) {
    line[strcspn(line, "\r\n")] = 0;
    if (!strncmp(line, "seed=", 5) && line[5]) {
      snprintf(seed, sizeof seed, "%s", line + 5);
      cfg_out->seed = seed;
    } else if (!strncmp(line, "starters=", 9)) {
      cfg_out->n_starters = 0;
      for (char* t = strtok(line + 9, ", "); t && cfg_out->n_starters < 6; t = strtok(NULL, ", "))
        cfg_out->starters[cfg_out->n_starters++] = atoi(t);
    }
  }
  fclose(f);
}

static void fatal(const char* msg) {
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

static void load_system_save(void) {
  gc_system_init_defaults(&sys_data);
  char* buf = NULL;
  size_t len = 0;
  bool used_bak = false;
  save_result res = save_read("system", &buf, &len, &used_bak);
  if (res == SAVE_OK && buf) {
    const char* why = NULL;
    gc_system_load(&sys_data, buf, len, &why);
    free(buf);
  }
}

static void save_system_save(void) {
  char buf[GC_SAVE_MAX];
  size_t len = gc_system_save(&sys_data, buf, sizeof buf);
  if (len > 0) {
    save_write("system", buf, len);
  }
}

static void start_new_run_in_slot(int slot) {
  current_slot = slot;
  rs_set_slot(slot);
  memset(&cfg, 0, sizeof cfg);
  cfg.entropy = (uint32_t)osGetTime();
  read_driver_file(&cfg);
  title_ui_close();
  save_slot_ui_close();
  rs_start(&cfg);
}

static void load_run_in_slot(int slot) {
  current_slot = slot;
  title_ui_close();
  save_slot_ui_close();
  if (!rs_load_slot(slot)) {
    start_new_run_in_slot(slot);
  }
}

static void open_save_slots_menu(void) {
  title_ui_close();
  save_slot_ui_events ev = {
    .on_load_slot = (void (*)(void*, int))load_run_in_slot,
    .on_new_slot = (void (*)(void*, int))start_new_run_in_slot,
    .on_delete_slot = NULL,
    .on_cancel = NULL,
    .ctx = NULL
  };
  save_slot_ui_open(&ev);
}

static void on_title_option(void* ctx, title_option opt) {
  (void)ctx;
  if (opt == TITLE_OPT_NEW_GAME) {
    if (slot_0_occupied) {
      open_save_slots_menu();
    } else {
      start_new_run_in_slot(0);
    }
  } else if (opt == TITLE_OPT_CONTINUE) {
    load_run_in_slot(title_continue_slot);
  } else if (opt == TITLE_OPT_SAVE_SLOTS) {
    open_save_slots_menu();
  }
}

static void open_title_screen(void) {
  save_slot_info slots[SAVE_SLOT_COUNT];
  save_slot_ui_scan_slots(slots);

  bool has_continue = false;
  title_continue_slot = 0;
  int wave = 1;
  const char* biome = "";

  slot_0_occupied = slots[0].exists;

  for (int i = 0; i < SAVE_SLOT_COUNT; i++) {
    if (slots[i].exists && slots[i].valid) {
      has_continue = true;
      title_continue_slot = i;
      wave = slots[i].wave;
      biome = slots[i].biome_name;
      break;
    }
  }

  title_ui_events ev = {
    .on_select_option = on_title_option,
    .ctx = NULL
  };
  title_ui_open(has_continue, title_continue_slot, wave, biome, &ev);
}

int main(void) {
  osSetSpeedupEnable(true);
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
  ps_init();

  load_system_save();
  open_title_screen();

  while (ui_tick()) {
    if (!title_ui_active() && !save_slot_ui_active() && !clear_ui_active() && !game_over_ui_active() && !rs_run()->wave) {
      open_title_screen();
    }
  }

  save_system_save();
  snd_exit();
  rr_exit();
  return 0;
}
