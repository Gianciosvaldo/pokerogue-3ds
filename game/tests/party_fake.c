/*
 * party_fake: the real party screen (menus/party.c) for the host test, with the drawing and text stubs it needs, so a run test can
 * release a member of a full party through the code the 3DS runs (party_open in PARTY_RELEASE mode, the options list, do_release,
 * release_done) instead of calling the driver's callback itself. Only the app's 4-line glue between party.c and its caller
 * (party_screen.c's on_select) is repeated here: party_screen.c also pulls in the summary and learn-move screens.
 *
 * Test code only: nothing here is part of the app.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menus.h"
#include "party_fake.h"
#include "pr/battle.h"

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

static char observed_text[128][192];
static int observed_count;
void pf_clear_text(void) { observed_count = 0; }
int pf_text_count(const char* text) {
  int n = 0;
  for (int i = 0; i < observed_count; i++) n += !strcmp(observed_text[i], text);
  return n;
}

int mui_text(mui_style s, float x, float y, const char* utf8) {
  (void)s; (void)x; (void)y;
  if (observed_count < 128) snprintf(observed_text[observed_count++], 192, "%s", utf8);
  return 0;
}

int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)ox; (void)oy;
  return mui_text(s, x, y, utf8);
}

int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color col, rr_color sh) {
  (void)col; (void)sh;
  return mui_text(s, x, y, utf8);
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

/* The party screen's message box: what it shows is counted (a farewell is menus' do_release) and the message ends at once, as
 * if the player pressed A */
static int g_farewells;
static bool is_farewell_text(const char* t) {
  static const char* const K[] = {"goodbye", "byebye", "farewell", "soLong", "thisIsWhereWePart", "illMissYou",
                                  "illNeverForgetYou", "untilWeMeetAgain", "sayonara", "smellYaLater"};
  if (strncmp(t, "partyUiHandler:", 15)) return false;
  for (size_t i = 0; i < sizeof K / sizeof *K; i++)
    if (!strcmp(t + 15, K[i])) return true;
  return false;
}
void mui_msg_show(mui_msg* msg, const char* text, int delay, mui_cb cb, void* ctx, bool prompt) {
  (void)delay, (void)prompt;
  memset(msg, 0, sizeof *msg);
  if (text && is_farewell_text(text)) g_farewells++;
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


/* ------------------------------------------------------------------ the screen */

static struct {
  void (*on_pick)(int slot, int option, void* arg);
  void* arg;
  int n;
} F;

/* party_screen.c's on_select */
static void on_select(void* ctx, int slot, int option) {
  (void)ctx;
  if (F.on_pick) F.on_pick(slot, option, F.arg);
}

void pf_open_release(const pr_battle* bt, void (*on_pick)(int slot, int option, void* arg), void* arg) {
  static menu_mon party[6];
  memset(party, 0, sizeof party);
  F.on_pick = on_pick;
  F.arg = arg;
  F.n = bt->n_party[0] < 6 ? bt->n_party[0] : 6;
  for (int i = 0; i < F.n; i++) {
    const pr_pokemon* p = bt->party[0][i];
    menu_mon* m = &party[i];
    m->species = (uint16_t)p->species;
    snprintf(m->name, sizeof m->name, "Species_%d", p->species);
    m->level = (uint16_t)p->level;
    m->hp = p->hp > 0 ? (uint32_t)p->hp : 0;
    m->max_hp = (uint32_t)p->stats[STAT_HP];
    m->allowed_in_battle = true;
  }
  party_args a;
  memset(&a, 0, sizeof a);
  a.mode = PARTY_RELEASE;
  a.battler_count = 1;
  static party_events ev;
  ev = (party_events){on_select, NULL, NULL, NULL, NULL, NULL, NULL};
  party_open(&a, party, F.n, &ev);
}

void pf_close(void) {
  party_close();
  F.on_pick = NULL;
}

bool pf_active(void) { return party_active() && F.on_pick; }

void pf_release(int slot) {
  if (slot < 0) {
    party_input(MB_CANCEL); /* B: back out of the list */
    return;
  }
  for (int i = 0; i < slot; i++) party_input(MB_DOWN);
  party_input(MB_ACTION); /* the options of that member: Release first */
  party_input(MB_ACTION); /* Release: do_release says goodbye, then release_done picks it */
}

int pf_farewells(void) { return g_farewells; }
