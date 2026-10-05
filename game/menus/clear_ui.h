/*
 * Clear Screen for PokeRogue 3DS (game/menus/clear_ui.h).
 */
#ifndef GAME_MENUS_CLEAR_UI_H
#define GAME_MENUS_CLEAR_UI_H

#include <stdbool.h>
#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int wave;
  int score;
  double money;
  int slot_idx;
  bool active;
} clear_ui_state;

void clear_ui_open(int wave, int score, double money, int slot_idx);
void clear_ui_close(void);
bool clear_ui_active(void);
bool clear_ui_input(mui_button b);
void clear_ui_update(float dt_ms);
void clear_ui_draw(void);

#ifdef __cplusplus
}
#endif

#endif
