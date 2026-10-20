/* The reward / shop screen and the biome map on the UI stack (see reward_host.h). */
#include "reward_host.h"

#include "ui.h"

static bool g_open;
static void (*g_menu)(void*);
static void* g_menu_arg;

static bool rh_input(ui_screen* s, ui_button b) {
  (void)s;
  if (b == BTN_MENU && shop_ui_active() && g_menu) {
    g_menu(g_menu_arg);
    return true;
  }
  const mui_button mb = (mui_button)b; /* both are upstream's Button enum, in order */
  if (shop_ui_active()) return shop_ui_input(mb);
  if (biome_map_active()) return biome_map_input(mb);
  return false;
}

/* A tap on the bottom screen (physical px): the shop's own touch slots, in canvas coordinates (the canvas starts 30 px down) */
static bool rh_touch(ui_screen* s, float x, float y) {
  (void)s;
  if (biome_map_active()) return biome_map_touch(x, y - 30);
  return shop_ui_active() && shop_ui_touch(x, y - 30);
}

static void rh_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  shop_ui_update(dt);
  biome_map_update(dt);
}

static void rh_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (shop_ui_active() && which == RR_BOTTOM) {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
    shop_ui_draw();
  } else if (biome_map_active() && which == RR_TOP) {
    mui_set_origin(RR_TOP, 40, 30); /* UI_TOP_CANVAS */
    biome_map_draw();
  } else if (biome_map_active() && which == RR_BOTTOM) {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
    biome_map_draw_bottom();
  }
}

static ui_screen g_screen = {"rewards", UI_OVERLAY, NULL, NULL, NULL, rh_input, rh_update, rh_draw, NULL, rh_touch};

static void put_on_stack(void) {
  if (g_open) return;
  g_open = true;
  ui_push(&g_screen);
}

void rh_open_shop(const shop_ui_args* args, const shop_ui_events* ev) {
  shop_ui_open(args, ev);
  put_on_stack();
}

void rh_open_biome_map(const biome_map_args* args, const biome_map_events* ev) {
  biome_map_open(args, ev);
  put_on_stack();
}

void rh_set_menu(void (*fn)(void*), void* arg) {
  g_menu = fn;
  g_menu_arg = arg;
}

void rh_close(void) {
  g_menu = NULL;
  g_menu_arg = NULL;
  shop_ui_close();
  biome_map_close();
  if (!g_open) return;
  g_open = false;
  if (ui_top() == &g_screen) ui_pop();
}
