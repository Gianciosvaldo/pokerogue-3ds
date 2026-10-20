/*
 * Biome Map selection UI (game/menus/biome_map.c).
 * Option selection dialog for biome choices when driven by game-core's Map choice state.
 *
 * Replaces OptionSelectUiHandler for SelectBiomePhase with Map item.
 */
#include "menus.h"

#include <stdio.h>
#include <string.h>

extern const char* const menu_biome_keys[];
extern const int menu_biome_key_count;

typedef struct {
  bool active;
  int cursor;
  int n_options;
  char labels[BIOME_MAP_MAX_OPTIONS][64];
  const char* label_ptrs[BIOME_MAP_MAX_OPTIONS];
  biome_map_events ev;
  float x, y, w;
} biome_map_state;

static biome_map_state g_map;

static const char* get_biome_name(uint8_t biome_id) {
  if (biome_id < menu_biome_key_count && menu_biome_keys[biome_id]) {
    char key[64];
    snprintf(key, sizeof key, "biome:%s", menu_biome_keys[biome_id]);
    return mui_t(key);
  }
  return mui_t("biome:unknownLocation");
}

void biome_map_open(const biome_map_args* args, const biome_map_events* ev) {
  memset(&g_map, 0, sizeof g_map);
  if (!args || !args->biomes || args->n_biomes <= 0) return;

  g_map.n_options = args->n_biomes > BIOME_MAP_MAX_OPTIONS ? BIOME_MAP_MAX_OPTIONS : args->n_biomes;
  if (ev) g_map.ev = *ev;

  float max_w = 0;
  for (int i = 0; i < g_map.n_options; i++) {
    const char* bname = get_biome_name(args->biomes[i]);
    snprintf(g_map.labels[i], sizeof g_map.labels[i], "%s", bname);
    g_map.label_ptrs[i] = g_map.labels[i];

    float w = (float)mui_text_width(TS_WINDOW, g_map.labels[i]);
    if (w > max_w) max_w = w;
  }

  g_map.w = max_w + 23.0f; /* WINDOW_PADDING matching OptionSelectUiHandler */
  if (g_map.n_options > 10) g_map.w += 5.0f;

  g_map.x = MUI_W - 1.0f;
  g_map.y = MUI_H - 48.0f; /* yOffset: 48 */
  g_map.cursor = 0;
  g_map.active = true;
}

void biome_map_close(void) {
  g_map.active = false;
}

bool biome_map_active(void) {
  return g_map.active;
}

bool biome_map_input(mui_button b) {
  if (!g_map.active) return false;

  if (b == MB_UP) {
    if (g_map.cursor > 0)
      g_map.cursor--;
    else
      g_map.cursor = g_map.n_options - 1;
    mui_sfx("ui/select");
    return true;
  }

  if (b == MB_DOWN) {
    if (g_map.cursor < g_map.n_options - 1)
      g_map.cursor++;
    else
      g_map.cursor = 0;
    mui_sfx("ui/select");
    return true;
  }

  if (b == MB_ACTION || b == MB_SUBMIT) {
    int chosen = g_map.cursor;
    mui_sfx("ui/select");
    if (g_map.ev.on_select) {
      g_map.ev.on_select(g_map.ev.ctx, chosen);
    }
    return true;
  }

  /* Upstream OptionSelectModeConfig blockCancelButton: true */
  if (b == MB_CANCEL) {
    return true;
  }

  return false;
}

void biome_map_update(float dt_ms) {
  (void)dt_ms;
}

/* The list as a window with its cursor, bottom edge at `bottom` in the canvas being drawn on */
static void list_rect(float bottom, float* left, float* top, float* h) {
  *h = (g_map.n_options + 1) * 16.0f - 2.0f;
  *left = g_map.x - g_map.w;
  *top = bottom - *h;
}

static void draw_list(float bottom) {
  float row_h = 16.0f, left, top, h;
  list_rect(bottom, &left, &top, &h);

  mui_window(left, top, g_map.w, h);

  for (int i = 0; i < g_map.n_options; i++) {
    mui_text(TS_WINDOW, left + 15.0f, top + 7.0f + i * row_h, g_map.label_ptrs[i]);
  }

  mui_image("ui/cursor", NULL, left + 10.0f, top + 15.0f + g_map.cursor * row_h, 0.5f, 0.5f);
}

void biome_map_draw(void) {
  if (!g_map.active) return;
  draw_list(g_map.y);
}

/* The bottom screen's copy of the list, where taps land: against the right edge and the bottom of the screen like the other lists there
 * (the canvas starts 30 px down the 240 px screen, so its y 208 is the screen's 238). */
#define BOTTOM_LIST_EDGE 208.0f

void biome_map_draw_bottom(void) {
  if (!g_map.active) return;
  draw_list(BOTTOM_LIST_EDGE);
}

/* A tap at (x, y) in the bottom canvas: a row is the cursor on it and A. */
bool biome_map_touch(float x, float y) {
  if (!g_map.active) return false;
  float left, top, h;
  list_rect(BOTTOM_LIST_EDGE, &left, &top, &h);
  if (x < left || x >= g_map.x || y < top + 7.0f || y >= top + 7.0f + g_map.n_options * 16.0f) return false;
  g_map.cursor = (int)((y - top - 7.0f) / 16.0f);
  return biome_map_input(MB_ACTION);
}
