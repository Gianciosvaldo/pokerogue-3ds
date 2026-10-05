/*
 * Game Over Screen for PokeRogue 3DS (game/menus/game_over_ui.h).
 */
#ifndef GAME_MENUS_GAME_OVER_UI_H
#define GAME_MENUS_GAME_OVER_UI_H

#include <stdbool.h>
#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int wave;
  int score;
  int slot_idx;
  bool active;
} game_over_ui_state;

void game_over_ui_open(int wave, int score, int slot_idx);
void game_over_ui_close(void);
bool game_over_ui_active(void);
bool game_over_ui_input(mui_button b);
void game_over_ui_update(float dt_ms);
void game_over_ui_draw(void);

#ifdef __cplusplus
}
#endif

#endif
