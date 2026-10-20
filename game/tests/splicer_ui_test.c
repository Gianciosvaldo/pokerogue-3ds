/*
 * splicer_ui_test.c: PC test harness for DNA Splicers target selection UI.
 * Tests PARTY_SPLICE mode in party.c: primary selection, secondary target
 * selection, options (PO_APPLY, PO_SPLICE, PO_UNSPLICE), filter handling, and cancellation.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"

/* Definition of RR_DEFAULT_OPTS for drawing mocks */
const rr_draw_opts RR_DEFAULT_OPTS = {0};

void rr_draw(const rr_frame* frame, float x, float y, const rr_draw_opts* opts) {
  (void)frame; (void)x; (void)y; (void)opts;
}

/* Mock translation and UI functions for PC harness */
void mui_sfx(const char* key) {
  (void)key;
}

const char* mui_t(const char* key) {
  if (strcmp(key, "partyUiHandler:choosePokemon") == 0) return "Choose a Pokémon.";
  if (strcmp(key, "partyUiHandler:doWhatWithThisPokemon") == 0) return "Do what with this Pokémon?";
  if (strcmp(key, "partyUiHandler:selectAnotherPokemonToSplice") == 0) return "Select another Pokémon to splice.";
  if (strcmp(key, "partyUiHandler:apply") == 0) return "Apply";
  if (strcmp(key, "partyUiHandler:splice") == 0) return "Splice";
  if (strcmp(key, "partyUiHandler:unsplice") == 0) return "Unsplice";
  if (strcmp(key, "partyUiHandler:cancel") == 0) return "Cancel";
  if (strcmp(key, "partyUiHandler:summary") == 0) return "Summary";
  if (strcmp(key, "partyUiHandler:unspliceConfirmation") == 0) return "Do you really want to unsplice?";
  return key;
}

char* mui_tf(char* out, size_t n, const char* key, ...) {
  snprintf(out, n, "%s", mui_t(key));
  return out;
}

int mui_text_width(mui_style s, const char* utf8) {
  (void)s;
  return (int)strlen(utf8) * 8;
}

float mui_text_height(mui_style s, const char* utf8) {
  (void)s;
  return 12.0f;
}

rr_color mui_style_color(mui_style s, bool shadow) {
  rr_color c = 0xFFFFFFFF;
  return c;
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

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color col, rr_color sh) {
  (void)s; (void)x; (void)y; (void)utf8; (void)col; (void)sh;
  return 0;
}

bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)key; (void)frame; (void)x; (void)y; (void)ox; (void)oy;
  return true;
}

bool mui_image_ex(const char* key, const char* frame, float x, float y, const rr_draw_opts* opts) {
  (void)key; (void)frame; (void)x; (void)y; (void)opts;
  return true;
}

const rr_frame* mui_frame(const char* key, const char* frame) {
  static rr_frame dummy = {0, 0, 100, 20, 0, 0, 100, 20};
  return &dummy;
}

void menu_draw_icon(const menu_mon* m, float x, float y, float ox, float oy) {
  (void)m; (void)x; (void)y; (void)ox; (void)oy;
}

void mui_fit_name(mui_style s, char* name, int max_w) {
  (void)s; (void)name; (void)max_w;
}

void mui_msg_show(mui_msg* msg, const char* text, int delay, mui_cb cb, void* ctx, bool prompt) {
  memset(msg, 0, sizeof *msg);
  if (cb) cb(ctx);
}

bool mui_msg_input(mui_msg* msg, mui_button b) {
  return false;
}

void mui_msg_update(mui_msg* msg, float dt_ms) {
  (void)msg; (void)dt_ms;
}

void mui_msg_draw(const mui_msg* msg, mui_style style, float x, float y) {
  (void)msg; (void)style; (void)x; (void)y;
}

int mui_msg_lines(const mui_msg* msg) {
  (void)msg;
  return 1;
}

void mui_confirm_show(mui_confirm* c, mui_cb yes, mui_cb no, void* ctx) {
  memset(c, 0, sizeof *c);
  c->active = true;
  c->yes = yes;
  c->no = no;
  c->ctx = ctx;
}

bool mui_confirm_input(mui_confirm* c, mui_button b) {
  if (!c->active) return false;
  if (b == MB_ACTION) {
    c->active = false;
    if (c->yes) c->yes(c->ctx);
    return true;
  }
  if (b == MB_CANCEL) {
    c->active = false;
    if (c->no) c->no(c->ctx);
    return true;
  }
  return false;
}

/* mui.c's: the box at the canvas' bottom-right, rows of 16 px from 7 px under its top (the widest of the two labels and 23 px wide) */
bool mui_confirm_touch(mui_confirm* c, float x, float y) {
  if (!c->active) return false;
  int w = mui_text_width(TS_WINDOW, mui_t("menu:yes")), w2 = mui_text_width(TS_WINDOW, mui_t("menu:no"));
  if (w2 > w) w = w2;
  const float bw = w + 23, bh = 3 * 16 - 2, left = 319 - bw, top = (180 - 48) - bh;
  if (x < left || x >= 319 || y < top + 7 || y >= top + 7 + 2 * 16) return false;
  c->cursor = (int)((y - top - 7) / 16);
  return mui_confirm_input(c, c->cursor ? MB_CANCEL : MB_ACTION); /* the fake's input: ACTION answers yes, CANCEL no */
}

void mui_confirm_draw(const mui_confirm* c) {
  (void)c;
}

/* Event tracking context */
typedef struct {
  int select_slot;
  int select_option;
  bool select_called;
} test_events;

static void test_on_select(void* ctx, int slot, int option) {
  test_events* ev = (test_events*)ctx;
  ev->select_slot = slot;
  ev->select_option = option;
  ev->select_called = true;
}

static const char* test_splicer_filter(void* ctx, int slot) {
  /* Prevent selecting a fused Pokemon as target */
  menu_mon* party = (menu_mon*)ctx;
  if (party[slot].fusion) return "This Pokémon is already fused.";
  return NULL;
}

int main(void) {
  printf("Running splicer_ui_test...\n");

  /* Setup 3 sample Pokemon:
   * Slot 0: Pikachu (unfused)
   * Slot 1: Charizard (unfused)
   * Slot 2: Venusaur + Blastoise (fused)
   */
  menu_mon party[3];
  memset(party, 0, sizeof party);

  snprintf(party[0].name, sizeof party[0].name, "Pikachu");
  party[0].species = 25;
  party[0].level = 50;
  party[0].hp = 100;
  party[0].max_hp = 100;
  party[0].allowed_in_battle = true;

  snprintf(party[1].name, sizeof party[1].name, "Charizard");
  party[1].species = 6;
  party[1].level = 55;
  party[1].hp = 120;
  party[1].max_hp = 120;
  party[1].allowed_in_battle = true;

  snprintf(party[2].name, sizeof party[2].name, "VenuBlast");
  party[2].species = 3;
  party[2].level = 60;
  party[2].hp = 150;
  party[2].max_hp = 150;
  party[2].fusion = true;
  party[2].fusion_species = 9;
  party[2].allowed_in_battle = true;

  test_events ev_ctx;
  memset(&ev_ctx, 0, sizeof ev_ctx);

  party_events ev = {0};
  ev.on_select = test_on_select;
  ev.ctx = &ev_ctx;

  party_args args = {0};
  args.mode = PARTY_SPLICE;
  args.battler_count = 1;
  args.filter = test_splicer_filter;

  /* ------------------------------------------------------------------------
   * Test 1: Splicer Primary -> Secondary Selection Flow
   * ------------------------------------------------------------------------ */
  printf("Test 1: Splicer Primary -> Secondary Selection Flow...\n");
  party_open(&args, party, 3, &ev);
  assert(party_active());

  /* Press ACTION on slot 0 (Pikachu). Options menu opens. */
  bool handled = party_input(MB_ACTION);
  assert(handled);

  /* Select option PO_APPLY (Apply) to initiate primary target selection */
  handled = party_input(MB_ACTION);
  assert(handled);

  /* Now in secondary selection mode (transfer_mode = true).
   * Move cursor down to slot 1 (Charizard). */
  handled = party_input(MB_DOWN);
  assert(handled);

  /* Press ACTION on slot 1 (Charizard). Options menu opens with PO_SPLICE. */
  handled = party_input(MB_ACTION);
  assert(handled);

  /* Press ACTION on PO_SPLICE option. */
  ev_ctx.select_called = false;
  handled = party_input(MB_ACTION);
  assert(handled);

  /* Verify on_select callback was called with primary slot 0 and secondary slot 1 */
  assert(ev_ctx.select_called);
  assert(ev_ctx.select_slot == 0);
  assert(ev_ctx.select_option == 1);
  printf("  -> Selected primary slot %d, secondary slot %d successfully.\n", ev_ctx.select_slot, ev_ctx.select_option);

  party_close();
  assert(!party_active());

  /* ------------------------------------------------------------------------
   * Test 2: Unsplice Option on Fused Pokemon
   * ------------------------------------------------------------------------ */
  printf("Test 2: Unsplice Option on Fused Pokemon...\n");
  memset(&ev_ctx, 0, sizeof ev_ctx);
  party_open(&args, party, 3, &ev);

  /* Move cursor down to slot 2 (fused VenuBlast) */
  party_input(MB_DOWN);
  party_input(MB_DOWN);

  /* Open options menu on slot 2 */
  party_input(MB_ACTION);

  /* Options list for fused Pokemon in PARTY_SPLICE mode has PO_APPLY, then PO_UNSPLICE */
  /* Move option cursor down to PO_UNSPLICE */
  party_input(MB_DOWN);

  /* Select PO_UNSPLICE */
  party_input(MB_ACTION);

  /* Confirm dialog pops up ("Do you really want to unsplice?") -> press ACTION to confirm */
  ev_ctx.select_called = false;
  party_input(MB_ACTION);

  /* Verify on_select was called with slot 2 and option PO_UNSPLICE */
  assert(ev_ctx.select_called);
  assert(ev_ctx.select_slot == 2);
  assert(ev_ctx.select_option == PO_UNSPLICE);
  printf("  -> Unsplice selected on slot %d successfully.\n", ev_ctx.select_slot);

  party_close();

  /* ------------------------------------------------------------------------
   * Test 3: Splicer Filter Prevention & Cancel Secondary Selection
   * ------------------------------------------------------------------------ */
  printf("Test 3: Cancel Secondary Selection...\n");
  memset(&ev_ctx, 0, sizeof ev_ctx);
  party_open(&args, party, 3, &ev);

  /* Pick slot 0 -> PO_APPLY */
  party_input(MB_ACTION);
  party_input(MB_ACTION);

  /* Press CANCEL to back out of secondary selection */
  party_input(MB_CANCEL);

  /* Press CANCEL again to exit party menu */
  ev_ctx.select_called = false;
  party_input(MB_CANCEL);

  assert(ev_ctx.select_called);
  assert(ev_ctx.select_slot == 6);
  assert(ev_ctx.select_option == PO_CANCEL);
  printf("  -> Cancelled secondary target selection and exited party screen.\n");

  party_close();

  /* ------------------------------------------------------------------------
   * Test 4: taps (party_touch, canvas coordinates). Slot 0 is the main slot (x 9..119, y 31..80), the benched ones are x 143..318, y 12..36 and
   * y 40..64; with no summary callback the options window (bottom right, rows of 16 px from 6 px under its top) holds the entry and Cancel
   * ------------------------------------------------------------------------ */
  printf("Test 4: Taps...\n");
  memset(&ev_ctx, 0, sizeof ev_ctx);
  party_open(&args, party, 3, &ev);
  assert(!party_touch(2.0f, 2.0f) && !ev_ctx.select_called); /* nothing there */
  assert(party_touch(60.0f, 50.0f) && !ev_ctx.select_called); /* slot 0: its options open */
  /* the window's padding takes nothing; the last pixel row of the Cancel row (y 156..172) closes the options without a callback */
  assert(party_touch(310.0f, 173.5f) && !ev_ctx.select_called);
  assert(party_touch(310.0f, 171.5f) && !ev_ctx.select_called);
  assert(party_touch(60.0f, 50.0f)); /* open again */
  /* a tap outside the options window is B: they close (a tap on the Cancel button then is not a row of the window) */
  assert(party_touch(30.0f, 100.0f) && !ev_ctx.select_called);
  assert(party_touch(291.0f, 164.0f));
  assert(ev_ctx.select_called && ev_ctx.select_slot == 6 && ev_ctx.select_option == PO_CANCEL);
  printf("  -> A tap outside the options closed them, and the Cancel button answered at once.\n");
  party_close();

  memset(&ev_ctx, 0, sizeof ev_ctx);
  party_open(&args, party, 3, &ev);
  assert(party_touch(60.0f, 50.0f)); /* slot 0 */
  assert(party_touch(310.0f, 155.5f)); /* the last pixel row of its first row, Apply: the secondary selection starts */
  assert(!ev_ctx.select_called);
  assert(!party_touch(142.0f, 20.0f)); /* just left of the benched slots */
  assert(party_touch(317.0f, 35.0f)); /* slot 1's bottom right corner: its options open (Splice) */
  assert(party_touch(310.0f, 140.5f)); /* the first pixel row of Splice */
  assert(ev_ctx.select_called && ev_ctx.select_slot == 0 && ev_ctx.select_option == 1);
  printf("  -> Taps on slot 0, Apply, slot 1 and Splice reached on_select(0, 1).\n");
  party_close();

  memset(&ev_ctx, 0, sizeof ev_ctx);
  party_open(&args, party, 3, &ev);
  assert(party_touch(200.0f, 50.0f)); /* slot 2, the fused one: Apply, Unsplice, Cancel */
  assert(party_touch(310.0f, 148.0f)); /* Unsplice */
  assert(!ev_ctx.select_called);
  assert(party_touch(300.0f, 115.0f)); /* the confirm box's No (y 109..125) answers it */
  assert(!ev_ctx.select_called);
  assert(party_touch(200.0f, 50.0f) && party_touch(310.0f, 148.0f));
  assert(party_touch(300.0f, 100.0f)); /* and Yes (y 93..109) */
  assert(ev_ctx.select_called && ev_ctx.select_slot == 2 && ev_ctx.select_option == PO_UNSPLICE);
  printf("  -> The unsplice question answers to taps on No and Yes.\n");
  party_close();

  printf("splicer_ui_test PASSED!\n");
  return 0;
}
