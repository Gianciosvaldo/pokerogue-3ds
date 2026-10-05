/*
 * Save Slots UI for PokeRogue 3DS (game/menus/save_slot_ui.h).
 * Manages 5 save slots (session_0..session_4), displaying run details or
 * corrupt/refusal status without auto-deleting files.
 */
#ifndef GAME_MENUS_SAVE_SLOT_UI_H
#define GAME_MENUS_SAVE_SLOT_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SAVE_SLOT_COUNT 5

typedef struct {
  bool exists;
  bool valid; /* false if corrupt or incompatible format */
  bool used_backup;
  int wave;
  int score;
  double money;
  int lead_species;
  int lead_level;
  char biome_name[32];
  char refusal_reason[64];
} save_slot_info;

typedef struct save_slot_ui_events {
  void (*on_load_slot)(void* ctx, int slot_idx);
  void (*on_new_slot)(void* ctx, int slot_idx);
  void (*on_delete_slot)(void* ctx, int slot_idx);
  void (*on_cancel)(void* ctx);
  void* ctx;
} save_slot_ui_events;

typedef struct {
  save_slot_info slots[SAVE_SLOT_COUNT];
  int selected_slot;
  int selected_sub_opt; /* 0: Play/Resume, 1: Delete, 2: Back */
  bool show_sub_menu;
  bool show_refusal_dialog;
  char refusal_msg[128];
  bool show_delete_confirm;
  save_slot_ui_events events;
  bool active;
} save_slot_ui_state;

void save_slot_ui_open(const save_slot_ui_events* ev);
void save_slot_ui_close(void);
bool save_slot_ui_active(void);
bool save_slot_ui_input(mui_button b);
bool save_slot_ui_touch(float touch_x, float touch_y);
void save_slot_ui_update(float dt_ms);
void save_slot_ui_draw(void);

/* Scans and populates info for all 5 save slots */
void save_slot_ui_scan_slots(save_slot_info out_slots[SAVE_SLOT_COUNT]);

#ifdef __cplusplus
}
#endif

#endif
