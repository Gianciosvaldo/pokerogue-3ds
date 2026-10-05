/* A party member's icon from a run summary (see party_icon.h). It is its own file because it uses menus' layer
 * (menus.h / mui.h); the screens that call it use the game framework's text API, and the two headers are not
 * included together in one file. */
#include "party_icon.h"

#include <string.h>

#include "menus.h"

void pi_draw(const rsv_mon* m, float x, float y, float scale) {
  menu_mon mm;
  memset(&mm, 0, sizeof mm);
  mm.species = m->species;
  mm.form = m->form;
  mm.gender = m->gender;
  mm.shiny = m->shiny;
  mm.variant = m->variant;
  menu_draw_icon_scaled(&mm, x, y, 0.5f, 0.0f, scale);
}
