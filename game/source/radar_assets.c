/* The Radar's door to the menus layer and battle-core's tables (see radar_assets.h). */
#include "radar_assets.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "menus.h"
#include "pr_data.h"

const pr_species* menu_species(uint16_t id); /* menus_data.c */
extern const char* const menu_nature_keys[25];

static menu_sprite g_sprite;
static bool g_loaded;

static void fill(menu_mon* m, int species, int form, int gender, bool shiny) {
  memset(m, 0, sizeof *m);
  m->species = (uint16_t)species;
  m->form = (uint8_t)form;
  m->gender = (int8_t)gender;
  m->shiny = shiny;
}

bool ra_species_types(int species, int form, int* type1, int* type2) {
  if (species < 1 || species > 0xFFFF) return false;
  const pr_species* sp = menu_species((uint16_t)species);
  if (!sp || form < 0 || form >= sp->nforms) return false;
  const pr_form* f = &pr_forms[sp->form0 + form];
  if (type1) *type1 = f->type1;
  if (type2) *type2 = f->type2;
  return true;
}

void ra_icon(int species, int form, int gender, bool shiny, float x, float y, float scale, rr_color tint) {
  menu_mon m;
  fill(&m, species, form, gender, shiny);
  menu_draw_icon_tinted(&m, x, y, 0.5f, 0.5f, scale, tint);
}

bool ra_sprite_load(int species, int form, int gender, bool shiny) {
  ra_sprite_free();
  menu_mon m;
  fill(&m, species, form, gender, shiny);
  g_loaded = menu_sprite_load(&g_sprite, &m, false);
  return g_loaded;
}

void ra_sprite_free(void) {
  if (g_loaded) menu_sprite_free(&g_sprite);
  g_loaded = false;
}

bool ra_sprite_draw(float x, float y, uint32_t ms, float max_h) {
  if (!g_loaded) return false;
  const rr_frame* f = menu_sprite_frame(&g_sprite, ms);
  if (!f) return false;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  if (max_h > 0 && f->src_h > max_h) o.scale_x = o.scale_y = max_h / (float)f->src_h;
  rr_draw(f, x, y, &o);
  return true;
}

const char* ra_move_name(int move) { return move > 0 && move < 0x10000 ? menu_move_name((uint16_t)move) : "???"; }

const char* ra_nature_name(int nature, char* out, size_t n) {
  if (n == 0) return "";
  out[0] = 0;
  if (nature < 0 || nature >= 25) return out;
  char key[48];
  snprintf(key, sizeof key, "nature:%s", menu_nature_keys[nature]);
  return i18n_t(out, (int)n, key, NULL);
}
