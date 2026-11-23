/* The Pokedex screen's icons and type labels from the menus layer (see pokedex_assets.h). */
#include "pokedex_assets.h"

#include <string.h>

#include "menus.h"
#include "mui.h"

extern const char* const menu_type_frames[];

void pxa_icon(int species, float x, float y, float scale, rr_color tint) {
  menu_mon m;
  memset(&m, 0, sizeof m);
  m.species = (uint16_t)species;
  m.gender = MG_MALE;
  menu_draw_icon_tinted(&m, x, y, 0.0f, 0.0f, scale, tint);
}

void pxa_type(int type, float x, float y) {
  if (type < 0 || type > 18) return;
  const char* f = menu_type_frames[type];
  if (f) mui_image("types", f, x, y, 0, 0);
}
