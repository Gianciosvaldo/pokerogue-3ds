/* me_screen_test: the Mystery Encounter question and its pick lists (source/me_screen.c, source/me_flow.c) on the host, over game-core's
 * real option checks, battle-core's real text formatter and a small locale (the lines of two real encounters), with a fake drawing layer.
 *
 *   make me_screen_test && build/me_screen_test
 *
 * Checks: the BBCode and the (+)/(-) colouring; the question shows the encounter's title, description, query, labels and the
 * highlighted option's tooltip with its tokens filled (the price in thousands, the Pokemon and the move named); a disabled option shows its
 * disabled label and tooltip, is dimmed and cannot be taken; an option that needs a Pokemon opens the party list (B goes back to the
 * question), a Pokemon that cannot be picked cannot be taken, the choice carries the party slot; Field Trip asks for the move too; the
 * "selected" text is said only when every token in it is known; a tap on a row takes it. Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/run.h"
#include "me_flow.h"
#include "me_screen.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

static int checks, failed;
#define CHECK(cond, ...)                                  \
  do {                                                    \
    checks++;                                             \
    if (!(cond)) {                                        \
      failed++;                                           \
      if (failed < 40) {                                  \
        printf("FAIL line %d: %s: ", __LINE__, #cond);    \
        printf(__VA_ARGS__);                              \
        printf("\n");                                     \
      }                                                   \
    }                                                     \
  } while (0)

/* ------------------------------------------------------------------ fakes */

typedef struct {
  float x, y;
  int screen;
  unsigned color;
  char text[400];
} drawn_t;
#define DRAWN_MAX 512
static drawn_t g_drawn[DRAWN_MAX];
static int n_drawn, g_which;
static ui_screen* g_stack[8];
static int g_depth, n_error;

void ui_push(ui_screen* s) { g_stack[g_depth++] = s; }
void ui_pop(void) { if (g_depth > 0) g_depth--; }
void ui_play_select(void) {}
void ui_play_error(void) { n_error++; }
bool ui_space(ui_space_id sp) {
  const bool top = sp == UI_FIELD || sp == UI_FIELD_UI || sp == UI_TOP_CANVAS || sp == UI_TOP_CANVAS_UI || sp == UI_PHYS_TOP;
  return top == (g_which == 0);
}
const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, 0xFFFFFFFFu};
static rr_frame g_frame;
#define N_REC 16
static char g_sil_key[N_REC][ME_SPRITE_LEN];
static unsigned g_sil_tint[N_REC];
static float g_sil_x[N_REC], g_sil_y[N_REC], g_sil_scale[N_REC];
static int n_sil;
const rr_frame* img_anim_frame(const char* key, int i) {
  (void)i;
  if (n_sil < N_REC) snprintf(g_sil_key[n_sil], ME_SPRITE_LEN, "%s", key);
  return &g_frame;
}
const rr_frame* mui_frame(const char* key, const char* frame) {
  if (n_sil < N_REC) snprintf(g_sil_key[n_sil], ME_SPRITE_LEN, "%s:%s", key, frame);
  return &g_frame;
}
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) {
  (void)f;
  if (n_sil < N_REC) {
    g_sil_tint[n_sil] = (unsigned)o->tint;
    g_sil_x[n_sil] = x;
    g_sil_y[n_sil] = y;
    g_sil_scale[n_sil] = o->scale_x;
  }
  n_sil++;
}
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }
void ui_window(float x, float y, float w, float h, win_variant v) { (void)x, (void)y, (void)w, (void)h, (void)v; }
void ui_cursor(float x, float y) { (void)x, (void)y; }
rr_color text_color(text_style st, bool shadow) { return (rr_color)(0x1000u + (unsigned)st * 2u + (shadow ? 1u : 0u)); }
text_opts text_defaults(void) {
  text_opts o;
  memset(&o, 0, sizeof o);
  o.chars = TEXT_ALL;
  return o;
}
float text_width(text_style st, const char* s, const text_opts* o) {
  (void)st, (void)o;
  return 6.0f * (float)strlen(s);
}
float text_line_width(text_style st, int font_px, const char* s, int nbytes) {
  (void)st, (void)font_px;
  return 6.0f * (float)(nbytes < 0 ? (int)strlen(s) : nbytes);
}
float text_line_pitch(text_style st, const text_opts* o) { (void)st, (void)o; return 12.0f; }
float hint_x(text_style st, float x0, const char* const* texts, int i) {
  float x = x0;
  for (int k = 0; k < i; k++) x += text_width(st, texts[k], NULL) + HINT_GAP;
  return x;
}
int hint_at(text_style st, float x0, const char* const* texts, int n, float x) {
  for (int i = 0; i < n; i++)
    if (x < hint_x(st, x0, texts, i + 1) - HINT_GAP / 2.0f) return i;
  return -1;
}
float text_draw(text_style st, float x, float y, const char* s, const text_opts* o) {
  (void)st;
  if (n_drawn < DRAWN_MAX) {
    drawn_t* d = &g_drawn[n_drawn++];
    d->x = x;
    d->y = y;
    d->screen = g_which;
    d->color = o ? (unsigned)o->color : 0;
    snprintf(d->text, sizeof d->text, "%s", s);
  }
  return 6.0f * (float)strlen(s);
}
/* lines of at most 40 characters, broken at a space */
void text_wrap(text_style st, const char* s, const text_opts* o, char* out, int n) {
  (void)st, (void)o;
  snprintf(out, (size_t)n, "%s", s);
}

const char* i18n_species_name(int id) { return id == 25 ? "Pikachu" : id == 131 ? "Lapras" : "Mon"; }
const char* i18n_move_name(int id) { return id == 57 ? "Surf" : id == 33 ? "Tackle" : "Move"; }

/* a small locale: the real lines of Shady Vitamin Dealer and Lost at Sea (English), and the names the texts name */
static const char* const SHADY =
    "{\"intro\":\"A man in a dark coat approaches you.\",\"title\":\"The Vitamin Dealer\","
    "\"description\":\"The man opens his jacket.\",\"query\":\"Which deal will you choose?\","
    "\"option\":{\"1\":{\"label\":\"The Cheap Deal\",\"tooltip\":\"(-) Pay {{option1Money, money}}\\n(-) Side Effects?\\n(+) Chosen Pokémon Gains 2 Random Vitamins\"},"
    "\"2\":{\"label\":\"The Pricey Deal\",\"tooltip\":\"(-) Pay {{option2Money, money}}\\n(+) Chosen Pokémon Gains 2 Random Vitamins\"},"
    "\"3\":{\"label\":\"Leave\",\"tooltip\":\"(-) No Rewards\",\"selected\":\"Heh, wouldn't have figured you for a coward.\"},"
    "\"selected\":\"The man hands you two bottles.${{selectedPokemon}} gained {{boost1}} and {{boost2}} boosts!\"}}";
static const char* const LOST =
    "{\"intro\":\"Wandering aimlessly through the sea.\",\"title\":\"Lost at Sea\",\"description\":\"The sea is turbulent.\","
    "\"query\":\"What will you do?\",\"option\":{\"1\":{\"label\":\"{{option1PrimaryName}} Might Help\","
    "\"labelDisabled\":\"Can't {{option1RequiredMove}}\",\"tooltip\":\"(+) {{option1PrimaryName}} saves you\","
    "\"tooltipDisabled\":\"You have no Pokémon to {{option1RequiredMove}} on\","
    "\"selected\":\"{{option1PrimaryName}} swims ahead.\"},\"2\":{\"label\":\"{{option2PrimaryName}} Might Help\","
    "\"labelDisabled\":\"Can't {{option2RequiredMove}}\",\"tooltip\":\"(+) {{option2PrimaryName}} saves you\","
    "\"tooltipDisabled\":\"You have no Pokémon to {{option2RequiredMove}} with\"},"
    "\"3\":{\"label\":\"Wander Aimlessly\",\"tooltip\":\"(-) Each of your Pokémon lose {{damagePercentage}}% of their total HP\"}}}";
static const char* const TRIP = "{\"title\":\"Field Trip\",\"description\":\"A teacher.\",\"query\":\"Which move?\","
                                "\"option\":{\"1\":{\"label\":\"A Physical Move\",\"tooltip\":\"(+) Show a move\","
                                "\"selected\":\"Let's see {{pokeName}}.\"}}}";
static const char* const POKEMON = "{\"pikachu\":\"Pikachu\",\"lapras\":\"Lapras\"}";
static const char* const MOVE = "{\"surf\":{\"name\":\"Surf\"},\"fly\":{\"name\":\"Fly\"}}";

static const char* locale_get(void* ctx, const char* file, int fallback, size_t* len) {
  (void)ctx;
  const char* r = NULL;
  if (fallback) return NULL;
  /* the file names the packs really hold (locales/en/mystery-encounters/<kebab name>-dialogue.json): a looser match here once let the
   * formatter ask for ".../lost-at-sea" and show "Option 1" with no text on a console */
  if (!strcmp(file, "mystery-encounters/shady-vitamin-dealer-dialogue")) r = SHADY;
  else if (!strcmp(file, "mystery-encounters/lost-at-sea-dialogue")) r = LOST;
  else if (!strcmp(file, "mystery-encounters/field-trip-dialogue")) r = TRIP;
  else if (!strcmp(file, "pokemon")) r = POKEMON;
  else if (!strcmp(file, "move")) r = MOVE;
  if (r) *len = strlen(r);
  return r;
}
const pr_text_source* be_text_source(void) {
  static pr_text_source src = {locale_get, NULL, "en"};
  return &src;
}
void be_format_message(const pr_msg* msg, char* out, size_t size) {
  pr_msg display = *msg;
  for (int i = 0; i < display.n_args; i++)
    if (display.args[i].kind == PR_ARG_MOVE) display.args[i].v |= PR_MSG_MOVE_BARE;
  pr_msg_format(&display, be_text_source(), out, size);
}

/* ------------------------------------------------------------------ helpers */

static gc_run R;
static me_choice got;
static int n_got;

static void chosen(const me_choice* c, void* arg) { (void)arg; got = *c; n_got++; }

static ui_screen* top(void) { return g_depth ? g_stack[g_depth - 1] : NULL; }
static void draw(int which) {
  n_drawn = 0;
  g_which = which;
  top()->draw(top(), which ? RR_BOTTOM : RR_TOP);
}
static bool has_text(int screen, const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == screen && strstr(g_drawn[i].text, needle)) return true;
  return false;
}
static const drawn_t* find_text(int screen, const char* needle) {
  for (int i = 0; i < n_drawn; i++)
    if (g_drawn[i].screen == screen && strstr(g_drawn[i].text, needle)) return &g_drawn[i];
  return NULL;
}
static void press(ui_button b) { top()->input(top(), b); }

static void mon(int i, int species, int level, int hp, int max_hp) {
  memset(&R.party[i], 0, sizeof R.party[i]);
  R.party[i].b.species = (uint16_t)species;
  R.party[i].b.is_player = true;
  R.party[i].b.level = level;
  R.party[i].b.hp = hp;
  R.party[i].b.stats[0] = max_hp;
  R.party_order[i] = (int8_t)i;
}

static void run_at(int type, int n_options) {
  memset(&R, 0, sizeof R);
  R.screen.kind = GC_SCREEN_MYSTERY;
  R.screen.mystery_type = (int8_t)type;
  R.screen.n_options = n_options;
  for (int i = 0; i < n_options; i++) R.screen.option_ok[i] = true;
  R.n_party = 2;
  mon(0, 25, 30, 40, 40);
  mon(1, 131, 32, 10, 80);
  g_depth = 0;
  n_got = 0;
  memset(&got, 0, sizeof got);
}

static void money_arg(int slot, const char* name, double v) {
  uint64_t bits;
  memcpy(&bits, &v, sizeof bits);
  R.screen.me_args[slot] = (pr_msg_arg){name, PR_ARG_MONEY_EN_US, (uint32_t)bits, NULL, (uint32_t)(bits >> 32)};
  if (R.screen.n_me_args <= slot) R.screen.n_me_args = slot + 1;
}

int main(void) {
  /* ---- the BBCode */
  {
    char plain[200];
    uint8_t k[200];
    size_t n = me_markup("(-) Pay @[MONEY]{1,500}\n(+) Gains @[TOOLTIP_TITLE]{Vitamins} (x)", plain, k, sizeof plain);
    CHECK(!strcmp(plain, "(-) Pay 1,500\n(+) Gains Vitamins (x)"), "plain: %s", plain);
    CHECK(n == strlen(plain), "length");
    CHECK(k[0] == ME_K_BLUE && k[2] == ME_K_BLUE && k[4] == ME_K_BLUE, "(-) segment is blue");
    CHECK(k[8] == ME_K_MONEY && k[12] == ME_K_MONEY, "the wrapper wins inside the segment");
    const char* plus = strstr(plain, "(+)");
    CHECK(k[plus - plain] == ME_K_GREEN, "(+) is green");
    const char* vit = strstr(plain, "Vitamins");
    CHECK(k[vit - plain] == ME_K_TITLE, "title run");
    const char* x = strstr(plain, "(x)");
    CHECK(k[x - plain] == ME_K_NORMAL, "a '(' ends the coloured segment");
    n = me_markup("a @[NOPE]{b} @[MONEY]{unclosed", plain, k, sizeof plain);
    CHECK(!strcmp(plain, "a b @[MONEY]{unclosed"), "unknown style keeps its text, unclosed stays: %s", plain);
    n = me_markup("0123456789", plain, k, 5);
    CHECK(n == 4 && !strcmp(plain, "0123"), "cut to the buffer");
  }

  /* ---- Shady Vitamin Dealer: the question, the tooltips with their prices */
  run_at(GC_ME_SHADY_VITAMIN_DEALER, 3);
  R.screen.option_pick[0] = R.screen.option_pick[1] = GC_PICK_MEMBER;
  money_arg(0, "option1Money", 1500);
  money_arg(1, "option2Money", 5000);
  char intro[400];
  me_flow_intro(&R, intro, sizeof intro);
  CHECK(!strcmp(intro, "A man in a dark coat approaches you."), "intro: %s", intro);
  CHECK(me_flow_open(&R, chosen, NULL), "opens");
  CHECK(g_depth == 1, "the question is up");
  draw(0);
  CHECK(has_text(0, "The Vitamin Dealer") && has_text(0, "The man opens his jacket.") && has_text(0, "Which deal will you choose?"),
        "top: title, description, query");
  draw(1);
  CHECK(has_text(1, "The Cheap Deal") && has_text(1, "The Pricey Deal") && has_text(1, "Leave"), "the three labels");
  CHECK(has_text(1, "1,500") && !has_text(1, "5,000"), "the highlighted tooltip names its price");
  const drawn_t* price = find_text(1, "(-) Pay 1,500");
  CHECK(price && price->color == (unsigned)text_color(TS_SUMMARY_BLUE, false), "the (-) segment is drawn in blue");
  const drawn_t* gain = find_text(1, "(+) Chosen");
  CHECK(gain && gain->color == (unsigned)text_color(TS_SUMMARY_GREEN, false), "the (+) segment is drawn in green");
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "5,000") && !has_text(1, "1,500"), "moving shows the next tooltip");
  press(BTN_DOWN);
  press(BTN_DOWN); /* stays on the last */
  draw(1);
  CHECK(has_text(1, "No Rewards"), "the last tooltip");
  CHECK(!find_text(1, "{{"), "no raw token is ever drawn");

  /* Leave: no pick; the "selected" line of the option is said, the general one (it needs boosts nobody knows) is not */
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 2 && g_depth == 0, "Leave answers at once and pops");
  CHECK(!strcmp(got.selected, "Heh, wouldn't have figured you for a coward."), "selected: %s", got.selected);

  /* the Cheap Deal opens the party; B goes back to the question on the same option; a Pokemon that cannot be picked is dimmed */
  CHECK(me_flow_open(&R, chosen, NULL), "opens again");
  press(BTN_CANCEL);
  CHECK(g_depth == 1 && n_got == 1, "B does nothing on the question: an encounter cannot be backed out of");
  press(BTN_ACTION);
  CHECK(g_depth == 1 && n_got == 1, "the party list is up, nothing answered");
  draw(1);
  CHECK(has_text(1, "Pikachu  Lv.30  40/40") && has_text(1, "Lapras  Lv.32  10/80"), "the party, as the player sees it");
  draw(0);
  CHECK(has_text(0, "Choose a Pokemon."), "the question of the list");
  press(BTN_CANCEL);
  CHECK(g_depth == 1, "back to the question");
  draw(1);
  CHECK(has_text(1, "The Cheap Deal"), "the question again");
  press(BTN_ACTION);
  press(BTN_DOWN);
  const int errors = n_error;
  press(BTN_ACTION); /* Lapras is under half HP: the cheap deal does not take it */
  CHECK(n_error == errors + 1 && n_got == 1 && g_depth == 1, "a Pokemon the deal rejects cannot be taken");
  press(BTN_UP);
  press(BTN_ACTION);
  CHECK(n_got == 2 && got.option == 0 && got.b == 0 && g_depth == 0, "Pikachu is slot 0");
  CHECK(!strcmp(got.selected, ""), "the general selected text needs boost1: left out (%s)", got.selected);

  /* the party order is the one the player sees: the second in line is slot 1 */
  R.party_order[0] = 1;
  R.party_order[1] = 0;
  CHECK(me_flow_open(&R, chosen, NULL), "again");
  press(BTN_DOWN);
  press(BTN_ACTION);                /* Pricey Deal */
  draw(1);
  CHECK(find_text(1, "Lapras")->y < find_text(1, "Pikachu")->y, "Lapras is first in line");
  press(BTN_ACTION);
  CHECK(n_got == 3 && got.option == 1 && got.b == 1, "the first row is the party slot 1 (Lapras): %d", got.b);
  R.party_order[0] = 0;
  R.party_order[1] = 1;

  /* a tap on a row takes it */
  CHECK(me_flow_open(&R, chosen, NULL), "again");
  CHECK(top()->touch(top(), 100, 6 + 24 * 2 + 5), "a tap on Leave");
  CHECK(n_got == 4 && got.option == 2, "the tap took it");

  /* ---- Lost at Sea: a disabled option shows its disabled texts, is dimmed and cannot be taken */
  run_at(GC_ME_LOST_AT_SEA, 3);
  R.screen.option_ok[1] = false;
  R.screen.n_me_args = 0;
  R.screen.me_args[R.screen.n_me_args++] = pr_arg_pokemon_name(NULL, "option1PrimaryName", &R.party[1].b);
  R.screen.me_args[R.screen.n_me_args++] = pr_arg_move("option1RequiredMove", 57);
  R.screen.me_args[R.screen.n_me_args++] = pr_arg_move("option2RequiredMove", 19);
  R.screen.me_args[R.screen.n_me_args++] = pr_arg_int("damagePercentage", 25);
  CHECK(me_flow_open(&R, chosen, NULL), "lost at sea opens");
  draw(1);
  CHECK(has_text(1, "Lapras Might Help"), "label with the Pokemon: first row");
  CHECK(has_text(1, "Can't Fly"), "the disabled label");
  CHECK(has_text(1, "Lapras saves you"), "tooltip of the first option");
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "You have no Pokémon to Fly with"), "the disabled tooltip");
  press(BTN_ACTION);
  CHECK(n_got == 0 && g_depth == 1, "the disabled option is not taken");
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "lose 25% of their total HP"), "the integer token");
  press(BTN_UP);
  press(BTN_UP);
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 0 && got.b == 0 && !strcmp(got.selected, "Lapras swims ahead."), "selected with the primary's name: %s",
        got.selected);

  /* ---- Field Trip: the Pokemon, then one of its moves */
  run_at(GC_ME_FIELD_TRIP, 3);
  R.party[0].b.n_moves = 2;
  R.party[0].b.moves[0] = 33;
  R.party[0].b.moves[1] = 57;
  R.screen.option_pick[0] = R.screen.option_pick[1] = R.screen.option_pick[2] = GC_PICK_MEMBER_MOVE;
  CHECK(me_flow_open(&R, chosen, NULL), "field trip opens");
  press(BTN_ACTION);
  press(BTN_ACTION); /* Pikachu */
  draw(1);
  CHECK(has_text(1, "Tackle") && has_text(1, "Surf"), "its moves");
  press(BTN_CANCEL);
  draw(1);
  CHECK(has_text(1, "Pikachu  Lv.30"), "B on the moves goes back to the party");
  press(BTN_ACTION);
  press(BTN_DOWN);
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 0 && got.b == 0 && got.c == 1, "the second move: %d %d %d", got.option, got.b, got.c);

  /* the Pillar Men: their own texts, three plain options, and three black silhouettes of the fighters on the top screen */
  run_at(GC_ME_PILLAR_MEN, 3);
  me_flow_intro(&R, intro, sizeof intro);
  CHECK(!strcmp(intro, "Three shadows rise in the dark.$A Bizarre Foe appears!"), "pillar intro: %s", intro);
  CHECK(me_flow_open(&R, chosen, NULL), "pillar question opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 3, "three silhouettes: %d", n_sil);
  CHECK(!strcmp(g_sil_key[0], "images/trainer/custom/esidisi") && !strcmp(g_sil_key[1], "images/trainer/custom/wamuu") &&
            !strcmp(g_sil_key[2], "images/trainer/custom/kars"),
        "in the order of the options");
  CHECK(g_sil_tint[0] == 0x000000FFu && g_sil_tint[1] == 0x000000FFu && g_sil_tint[2] == 0x000000FFu, "drawn black");
  draw(1);
  CHECK(has_text(1, "Esidisi") && has_text(1, "Wamuu") && has_text(1, "Kars"), "the three fighters");
  CHECK(has_text(1, "easy fight"), "the highlighted tooltip");
  press(BTN_DOWN);
  press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "brutal fight"), "the last tooltip");
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 2 && got.b == 0 && !strcmp(got.selected, "You step up to Kars!"), "Kars: %d '%s'", got.option, got.selected);
  /* an encounter that is not the Pillar Men draws no silhouettes */
  run_at(GC_ME_SLUMBERING_SNORLAX, 3);
  CHECK(me_flow_open(&R, chosen, NULL), "another encounter opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 0, "no silhouettes elsewhere");

  /* intro sprites, laid out as upstream's MysteryEncounterIntroVisuals does */
  run_at(GC_ME_FIELD_TRIP, 3); /* three sprites without an offset: spaced over -40..40, 27 px apart, centre 4 right of the middle */
  CHECK(me_flow_open(&R, chosen, NULL), "field trip opens for its sprites");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 3, "field trip: three sprites: %d", n_sil);
  CHECK(!strcmp(g_sil_key[0], "images/trainer/preschooler_m") && !strcmp(g_sil_key[1], "images/mystery-encounters/field_trip_teacher") &&
            !strcmp(g_sil_key[2], "images/trainer/preschooler_f"),
        "field trip: the keys: %s %s %s", g_sil_key[0], g_sil_key[1], g_sil_key[2]);
  CHECK(g_sil_x[0] == 160 + -40 + 0.5f * 27 + 4 && g_sil_x[1] == 160 + -40 + 1.5f * 27 + 4 && g_sil_x[2] == 160 + -40 + 2.5f * 27 + 4,
        "field trip: spaced: %g %g %g", g_sil_x[0], g_sil_x[1], g_sil_x[2]);
  CHECK(g_sil_y[0] == 164 && g_sil_tint[0] == 0xFFFFFFFFu && g_sil_scale[0] == 1, "field trip: floor, no shading, native size");
  run_at(GC_ME_LOST_AT_SEA, 3); /* one sprite with an offset */
  CHECK(me_flow_open(&R, chosen, NULL), "lost at sea opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 1 && g_sil_x[0] == 160 + 4 + 20 && g_sil_y[0] == 164 + 3, "buoy at its offset: %d %g %g", n_sil, g_sil_x[0], g_sil_y[0]);
  run_at(GC_ME_THE_POKEMON_SALESMAN, 3); /* the run-time Pokemon keeps its place: one sprite drawn, spaced as one of two */
  CHECK(me_flow_open(&R, chosen, NULL), "salesman opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 1 && g_sil_x[0] == 160 + -40 + 0.5f * 40 + 4, "salesman at the left half: %d %g", n_sil, g_sil_x[0]);
  run_at(GC_ME_TRASH_TO_TREASURE, 3); /* scaled, shaded */
  CHECK(me_flow_open(&R, chosen, NULL), "trash opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 1 && !strcmp(g_sil_key[0], "images/pokemon/569-gigantamax") && g_sil_scale[0] == 1.5f && g_sil_tint[0] == 0x999999FFu,
        "garbodor: %d %s %g %08x", n_sil, g_sil_key[0], g_sil_scale[0], g_sil_tint[0]);
  run_at(GC_ME_ABSOLUTE_AVARICE, 3); /* 11 item icons from the items atlas, at the item base (-19, +2) plus their offsets */
  CHECK(me_flow_open(&R, chosen, NULL), "avarice opens");
  n_sil = 0;
  draw(0);
  CHECK(n_sil == 11 && !strcmp(g_sil_key[0], "items:lum_berry") && g_sil_x[0] == 160 + 4 + 7 && g_sil_y[0] == 164 + 2 - 14,
        "avarice berries: %d %s %g %g", n_sil, g_sil_key[0], g_sil_x[0], g_sil_y[0]);
  {
    me_intro_sprite sp[ME_MAX_INTRO];
    int drawn = 0;
    for (int t = 0; t < GC_ME_COUNT; t++) { /* nothing overflows, nothing is placed off the screen, every key is a known folder */
      const int n = me_flow_intro_sprites(t, 160, 164, sp, ME_MAX_INTRO);
      drawn += n > 0;
      for (int i = 0; i < n; i++)
        CHECK(sp[i].x > 40 && sp[i].x < 280 && sp[i].y > 100 && sp[i].y < 200 &&
                  (!strncmp(sp[i].key, "images/", 7) || !strcmp(sp[i].key, "items")),
              "sprite %d of encounter %d placed on the screen: %s %g %g", i, t, sp[i].key, sp[i].x, sp[i].y);
    }
    CHECK(drawn == 23, "23 encounters draw an intro: %d", drawn);
  }

  /* a screen that is not an encounter does not open */
  R.screen.kind = GC_SCREEN_COMMAND;
  CHECK(!me_flow_open(&R, chosen, NULL), "only on an encounter screen");
  CHECK(me_flow_ns(GC_ME_LOST_AT_SEA) && !strcmp(me_flow_ns(GC_ME_LOST_AT_SEA), "lostAtSea") && !me_flow_ns(GC_ME_COUNT),
        "namespace table");

  /* every encounter's namespace names a real upstream locale file (the 31 files of locales 270ed0a; Pillar Men have none, their text is in the app) */
  {
    static const char* const FILES[GC_ME_COUNT] = {
        "mysterious-challengers", "mysterious-chest", "dark-deal", "fight-or-flight", "slumbering-snorlax", "training-session",
        "department-store-sale", "shady-vitamin-dealer", "field-trip", "safari-zone", "lost-at-sea", "fiery-fallout",
        "the-strong-stuff", "the-pokemon-salesman", "an-offer-you-cant-refuse", "delibirdy", "absolute-avarice", "a-trainers-test",
        "trash-to-treasure", "berries-abound", "clowning-around", "part-timer", "dancing-lessons", "weird-dream",
        "the-winstrate-challenge", "teleporting-hijinks", "bug-type-superfan", "fun-and-games", "uncommon-breed",
        "global-trade-system", "the-expert-pokemon-breeder", NULL};
    for (int t = 0; t < GC_ME_COUNT; t++) {
      char ns[96], file[96], want[96];
      if (!FILES[t]) continue;
      snprintf(ns, sizeof ns, "mysteryEncounters/%s", me_flow_ns(t));
      snprintf(want, sizeof want, "mystery-encounters/%s-dialogue", FILES[t]);
      pr_text_ns_file(ns, file, sizeof file);
      CHECK(!strcmp(file, want), "namespace -> locale file");
      if (strcmp(file, want)) printf("  %s -> %s, wanted %s\n", ns, file, want);
    }
  }

  printf("me_screen_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
