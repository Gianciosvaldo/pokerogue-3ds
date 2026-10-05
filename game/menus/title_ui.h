/*
 * Title UI for PokeRogue 3DS (game/menus/title_ui.h).
 * Handles title screen menu navigation on top and bottom screens.
 */
#ifndef GAME_MENUS_TITLE_UI_H
#define GAME_MENUS_TITLE_UI_H

#include <stdbool.h>
#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TITLE_OPT_NEW_GAME = 0,
  TITLE_OPT_CONTINUE,
  TITLE_OPT_SAVE_SLOTS,
  TITLE_OPT_COUNT
} title_option;

typedef struct title_ui_events {
  void (*on_select_option)(void* ctx, title_option opt);
  void* ctx;
} title_ui_events;

typedef struct {
  bool has_continue_save;
  int continue_slot;
  int continue_wave;
  char continue_biome[32];
  title_ui_events events;
  int selected_opt;
  bool active;
} title_ui_state;

void title_ui_open(bool has_continue, int continue_slot, int continue_wave, const char* continue_biome, const title_ui_events* ev);
void title_ui_close(void);
bool title_ui_active(void);
bool title_ui_input(mui_button b);
bool title_ui_touch(float touch_x, float touch_y);
void title_ui_update(float dt_ms);
void title_ui_draw(void);

#ifdef __cplusplus
}
#endif

#endif
