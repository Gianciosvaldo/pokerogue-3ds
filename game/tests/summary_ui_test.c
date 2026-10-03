/*
 * summary_ui_test.c: PC test harness for Summary UI, Shiny Tier parsing, and Passive Ability slot.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1.0f, 1.0f, 0, 0xffffffff};

void rr_draw(const rr_frame* frame, float x, float y, const rr_draw_opts* opts) {
  (void)frame; (void)x; (void)y; (void)opts;
}

void rr_draw_cropped(const rr_frame* frame, float x, float y, int crop_w, int crop_h, const rr_draw_opts* opts) {
  (void)frame; (void)x; (void)y; (void)crop_w; (void)crop_h; (void)opts;
}

/* Stub functions required by mui when building on PC without 3DS libraries */
void mui_sfx(const char* key) {
  (void)key;
}

const char* mui_t(const char* key) {
  return key;
}

int mui_text_width(mui_style s, const char* utf8) {
  (void)s;
  return (int)strlen(utf8) * 8;
}

float mui_text_height(mui_style s, const char* utf8) {
  (void)s; (void)utf8;
  return 12.0f;
}

void mui_window(float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  (void)s; (void)x; (void)y; (void)utf8;
  return 0;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)s; (void)x; (void)y; (void)ox; (void)oy; (void)utf8;
  return 0;
}

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color sh) {
  (void)s; (void)x; (void)y; (void)utf8; (void)c; (void)sh;
  return 0;
}

int mui_text_ls(mui_style s, float x, float y, float pitch, const char* utf8, rr_color c, rr_color sh) {
  (void)s; (void)x; (void)y; (void)pitch; (void)utf8; (void)c; (void)sh;
  return 0;
}

int mui_bbtext(mui_style s, float x, float y, const char* bbtext) {
  (void)s; (void)x; (void)y; (void)bbtext;
  return 0;
}

void mui_clip(float x, float y, float w, float h) {
  (void)x; (void)y; (void)w; (void)h;
}

void mui_clip_off(void) {}

bool mui_image(const char* atlas, const char* frame, float x, float y, float ox, float oy) {
  (void)atlas; (void)frame; (void)x; (void)y; (void)ox; (void)oy;
  return true;
}

bool mui_image_ex(const char* atlas, const char* frame, float x, float y, const rr_draw_opts* opts) {
  (void)atlas; (void)frame; (void)x; (void)y; (void)opts;
  return true;
}

const rr_frame* mui_frame(const char* key, const char* frame) {
  (void)key; (void)frame;
  static rr_frame dummy = {0, 0, 16, 16};
  return &dummy;
}

int mui_wrap(mui_style s, const char* text, float max_w, char* out, size_t out_len) {
  (void)s; (void)max_w;
  if (text && out) {
    snprintf(out, out_len, "%s", text);
  }
  return 1;
}

char* mui_tf(char* out, size_t len, const char* key, ...) {
  if (out) snprintf(out, len, "%s", key);
  return out;
}

char* mui_bbfrag(char* out, size_t len, const char* text, mui_style style, bool close) {
  if (out) snprintf(out, len, "%s", text ? text : "");
  return out;
}

rr_color mui_style_color(mui_style s, bool shadow) {
  (void)s; (void)shadow;
  return 0xffffffff;
}

/* Menu sprite stubs */
bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back) {
  (void)m; (void)back;
  if (s) s->n = 1;
  return true;
}

void menu_sprite_free(menu_sprite* s) {
  if (s) s->n = 0;
}

const rr_frame* menu_sprite_frame(const menu_sprite* s, uint32_t ms) {
  (void)s; (void)ms;
  static rr_frame dummy = {0, 0, 32, 32};
  return &dummy;
}

typedef struct {
  bool closed;
  int swap_slot, swap_a, swap_b;
} test_events;

static void on_test_close(void* ctx) {
  test_events* e = (test_events*)ctx;
  e->closed = true;
}

static void on_test_swap(void* ctx, int slot, int a, int b) {
  test_events* e = (test_events*)ctx;
  e->swap_slot = slot;
  e->swap_a = a;
  e->swap_b = b;
}

int main(void) {
  printf("Running summary_ui_test...\n");

  /* 1. Test Shiny Tier Parsing */
  menu_mon mon;
  memset(&mon, 0, sizeof mon);
  mon.species = 25; /* Pikachu */
  mon.level = 50;

  /* Non-shiny */
  int fus_tier = -1;
  int tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 0);
  assert(fus_tier == 0);
  assert(!strcmp(summary_shiny_tier_name(tier), "None"));
  printf("Shiny Tier (Non-shiny -> None) PASSED.\n");

  /* Yellow Shiny (Tier 1) */
  mon.shiny = true;
  mon.variant = 0;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 1);
  assert(!strcmp(summary_shiny_tier_name(tier), "Yellow"));
  printf("Shiny Tier (Variant 0 -> Yellow) PASSED.\n");

  /* Blue Shiny (Tier 2) */
  mon.variant = 1;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 2);
  assert(!strcmp(summary_shiny_tier_name(tier), "Blue"));
  printf("Shiny Tier (Variant 1 -> Blue) PASSED.\n");

  /* Red Shiny (Tier 3) */
  mon.variant = 2;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 3);
  assert(!strcmp(summary_shiny_tier_name(tier), "Red"));
  printf("Shiny Tier (Variant 2 -> Red) PASSED.\n");

  /* Fused Double Shiny (Base Red, Fusion Blue) */
  mon.fusion = true;
  mon.fusion_shiny = true;
  mon.fusion_variant = 1;
  tier = summary_mon_shiny_tier(&mon, &fus_tier);
  assert(tier == 3);
  assert(fus_tier == 2);
  assert(!strcmp(summary_shiny_tier_name(tier), "Red"));
  assert(!strcmp(summary_shiny_tier_name(fus_tier), "Blue"));
  printf("Shiny Tier (Fused Double Shiny Red+Blue) PASSED.\n");

  /* 2. Test Summary Screen Open & Profile Page Rendering */
  menu_mon party[2];
  memset(party, 0, sizeof party);

  /* Mon 0: No passive ability */
  party[0].species = 6; /* Charizard */
  party[0].ability = 66; /* Blaze */
  party[0].passive = 0;
  party[0].has_passive = false;
  snprintf(party[0].name, sizeof party[0].name, "Charizard");

  /* Mon 1: Has passive ability */
  party[1].species = 257; /* Blaziken */
  party[1].ability = 66; /* Blaze */
  party[1].passive = 3;  /* Speed Boost */
  party[1].has_passive = true;
  snprintf(party[1].name, sizeof party[1].name, "Blaziken");

  test_events ev_data;
  memset(&ev_data, 0, sizeof ev_data);

  summary_args args;
  memset(&args, 0, sizeof args);
  args.mode = SUMMARY_DEFAULT;
  args.page = SUMMARY_PROFILE;
  args.party = party;
  args.n = 2;
  args.index = 0;
  args.from_party = true;

  summary_events ev;
  ev.on_close = on_test_close;
  ev.on_move_swap = on_test_swap;
  ev.ctx = &ev_data;

  summary_open(&args, &ev);
  assert(summary_active());
  printf("Summary screen opened on Mon 0 (no passive).\n");

  /* Render profile page for Mon 0 */
  summary_update(300.0f);
  summary_draw();
  printf("Profile page drawn for Mon 0 (primary ability only).\n");

  /* Navigate D-pad Down to Mon 1 (has passive) */
  bool handled = summary_input(MB_DOWN);
  assert(handled);
  summary_update(300.0f);
  summary_draw();
  printf("Profile page drawn for Mon 1 (primary + passive slot rendered).\n");

  /* Navigate Right to Stats page */
  handled = summary_input(MB_RIGHT);
  assert(handled);
  summary_update(300.0f);
  summary_draw();

  /* Navigate Right to Moves page */
  handled = summary_input(MB_RIGHT);
  assert(handled);
  summary_update(300.0f);
  summary_draw();

  /* Cancel move selection mode */
  handled = summary_input(MB_CANCEL);
  assert(handled);

  /* Cancel summary screen */
  handled = summary_input(MB_CANCEL);
  assert(handled);
  assert(ev_data.closed);
  assert(!summary_active());
  printf("Summary screen closed successfully.\n");

  printf("summary_ui_test PASSED ALL CHECKS!\n");
  return 0;
}
