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

#include "../../game-core/src/internal.h"
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
int text_lines(text_style st, const char* s, const text_opts* o) { (void)st, (void)o; int n = 1; for (; *s; s++) n += *s == '\n'; return n; }
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
const char* i18n_ability_name(int id) { static char b[24]; snprintf(b, sizeof b, "Ability%d", id); return b; }
const char* i18n_t(char* out, int n, const char* key, ...) { snprintf(out, (size_t)n, "%s", key); return out; }
const char* i18n_move_name(int id) { return id == 57 ? "Surf" : id == 33 ? "Tackle" : "Move"; }

/* a small locale: the real lines of Shady Vitamin Dealer and Lost at Sea (English), and the names the texts name */
static const char* const SHADY =
    "{\"intro\":\"A man in a dark coat approaches you.\",\"title\":\"The Vitamin Dealer\","
    "\"description\":\"The man opens his jacket.\",\"query\":\"Which deal will you choose?\","
    "\"option\":{\"1\":{\"label\":\"The Cheap Deal\",\"tooltip\":\"(-) Pay {{option1Money, money}}\\n(-) Side Effects?\\n(+) Chosen Pokémon Gains 2 Random Vitamins\"},"
    "\"2\":{\"label\":\"The Pricey Deal\",\"tooltip\":\"(-) Pay {{option2Money, money}}\\n(+) Chosen Pokémon Gains 2 Random Vitamins\"},"
    "\"3\":{\"label\":\"Leave\",\"tooltip\":\"(-) No Rewards\",\"selected\":\"Heh, wouldn't have figured you for a coward.\"},"
    "\"selected\":\"The man hands you two bottles.${{selectedPokemon}} gained {{boost1}} and {{boost2}} boosts!\"}}";
static const char* const TELE =
    "{\"title\":\"Teleporting Hijinks\",\"option\":{\"1\":{\"label\":\"Put Money In\",\"tooltip\":\"(-) Pay {{price, money}}\"},"
    "\"2\":{\"label\":\"A Pokémon Helps\",\"tooltip\":\"(-) {{option2PrimaryName}} Helps\"},\"3\":{\"label\":\"Inspect the Machine\",\"tooltip\":\"(-) Pokémon Battle\"}}}";
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

#include "fixtures/mystery-encounters-en.inc"
static bool use_source_locales;

static const char* locale_get(void* ctx, const char* file, int fallback, size_t* len) {
  (void)ctx;
  const char* r = NULL;
  if (fallback) return NULL;
  if (use_source_locales)
    for (size_t i = 0; i < sizeof source_locales / sizeof *source_locales; i++)
      if (!strcmp(file, source_locales[i].file)) {
        *len = strlen(source_locales[i].json);
        return source_locales[i].json;
      }
  /* the file names the packs really hold (locales/en/mystery-encounters/<kebab name>-dialogue.json): a looser match here once let the
   * formatter ask for ".../lost-at-sea" and show "Option 1" with no text on a console */
  if (!strcmp(file, "mystery-encounters/shady-vitamin-dealer-dialogue")) r = SHADY;
  else if (!strcmp(file, "mystery-encounters/lost-at-sea-dialogue")) r = LOST;
  else if (!strcmp(file, "mystery-encounters/teleporting-hijinks-dialogue")) r = TELE;
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
  if (!top()) { CHECK(false, "draw requires an open screen"); return; }
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
static void press(ui_button b) { if (!top()) { CHECK(false, "input requires an open screen"); return; } top()->input(top(), b); }

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
  /* Actual source labels, the cost token and both battle-introduction pages. */
  use_source_locales=true;run_at(GC_ME_TRASH_TO_TREASURE,2);R.n_party=3;mon(2,SPECIES_ID_MEW,30,40,40);
  gc_mystery_setup_screen(&R);
  CHECK(R.screen.n_options==2&&R.screen.option_ok[0]&&R.screen.option_ok[1],"both trash options work with an empty inventory");
  CHECK(me_flow_open(&R,chosen,NULL),"trash source question opens");press(BTN_DOWN);draw(1);
  CHECK(has_text(1,"Dig for Valuables")&&has_text(1,"2.5"),"digging shows source reward/cost description");
  press(BTN_UP);press(BTN_ACTION);
  CHECK(n_got==1&&got.option==0&&strstr(got.selected,"You wander around the heap")&&strstr(got.selected,"Suddenly, the garbage shifts!"),"investigation includes the Garbodor reveal: %s",got.selected);
  use_source_locales=false;
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

  /* Training Session: the Pokemon, then one of the 25 natures (scrolling list) or its abilities */
  run_at(GC_ME_TRAINING_SESSION, 3);
  R.screen.option_pick[1] = GC_PICK_MEMBER_NATURE;
  R.screen.option_pick[2] = GC_PICK_MEMBER_ABILITY;
  CHECK(me_flow_open(&R, chosen, NULL), "training opens");
  press(BTN_DOWN);
  press(BTN_ACTION);
  draw(1);
  CHECK(has_text(1, "Pikachu  Lv.30"), "the party list first");
  press(BTN_ACTION); /* Pikachu */
  draw(1);
  CHECK(has_text(1, "nature:hardy (new)") && !has_text(1, "nature:nature"), "the nature list starts at Hardy, new without a dex");
  for (int i = 0; i < 24; i++) press(BTN_DOWN);
  press(BTN_ACTION); /* the 25th */
  CHECK(n_got == 1 && got.option == 1 && got.b == 0 && got.c == 24, "last nature: %d %d %d", got.option, got.b, got.c);
  CHECK(!strcmp(gc_nature_key(24), "nature:quirky"), "nature key table");
  run_at(GC_ME_TRAINING_SESSION, 3);
  R.screen.option_pick[1] = GC_PICK_MEMBER_NATURE;
  R.screen.option_pick[2] = GC_PICK_MEMBER_ABILITY;
  CHECK(me_flow_open(&R, chosen, NULL), "training opens again");
  press(BTN_DOWN);
  press(BTN_DOWN);
  press(BTN_ACTION);
  press(BTN_ACTION);
  draw(1);
  CHECK(has_text(1, "Ability9"), "Pikachu's first ability (Static): %s", "list");
  press(BTN_CANCEL);
  draw(1);
  CHECK(has_text(1, "Pikachu  Lv.30"), "B on the abilities goes back to the party");
  press(BTN_ACTION);
  press(BTN_DOWN);
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 2 && got.b == 0 && (got.c == 1 || got.c == 2), "an ability slot: %d %d %d", got.option, got.b, got.c);

  /* Teleporting Hijinks (texts not ported): tokens the screen does not carry never show their braces */
  run_at(GC_ME_TELEPORTING_HIJINKS, 3);
  CHECK(me_flow_open(&R, chosen, NULL), "teleporting opens");
  press(BTN_DOWN);
  draw(1);
  CHECK(!has_text(1, "{{") && has_text(1, "a Pokemon Helps"), "no raw tokens in the tooltip");

  /* The GTS must not silently pick offer zero. Exercise the actual generated offers and choice encoding. */
  run_at(GC_ME_GLOBAL_TRADE_SYSTEM, 4);
  gc_run_new(&R, GAME_MODES_CLASSIC, "mystery-ui-gts", 812);
  gc_starter starters[2] = {{.species = SPECIES_ID_PIKACHU, .tera_type = -1}, {.species = SPECIES_ID_LAPRAS, .tera_type = -1}};
  for (int k = 0; k < 2; k++) for (int j = 0; j < 6; j++) starters[k].ivs[j] = 15;
  const char* why = NULL;
  CHECK(gc_run_set_starters(&R, starters, 2, &why), "real GTS party: %s", why ? why : "");
  R.wave = 37;
  pr_run_reset_seed(&R.run, R.wave);
  R.mystery_type = R.screen.mystery_type = GC_ME_GLOBAL_TRADE_SYSTEM;
  R.screen.kind = GC_SCREEN_MYSTERY;
  gc_mystery_setup_screen(&R);
  CHECK(R.screen.option_pick[0] == GC_PICK_MEMBER_TRADE && R.screen.option_pick[2] == GC_PICK_MEMBER_ITEM, "both GTS second picks exposed");
  const pr_rng before_preview = R.run.rng;
  for (int offer = 0; offer < 3; offer++) {
    g_depth = n_got = 0;
    CHECK(me_flow_open(&R, chosen, NULL), "GTS offer %d", offer);
    press(BTN_ACTION); /* party */
    press(BTN_ACTION); /* member 0 */
    draw(0);
    CHECK(has_text(0, "Choose a trade offer."), "GTS asks before committing");
    CHECK(n_got == 0, "no automatic trade");
    press(BTN_CANCEL);
    CHECK(n_got == 0 && g_depth == 1, "back to party without a trade");
    press(BTN_ACTION);
    for (int i = 0; i < offer; i++) press(BTN_DOWN);
    press(BTN_ACTION);
    CHECK(n_got == 1 && got.option == 0 && got.b == 0 && got.c == offer, "GTS offer ordinal %d transmitted (%d)", offer, got.c);
    CHECK(!memcmp(&before_preview, &R.run.rng, sizeof before_preview), "preview and navigation preserve run RNG");
  }
  /* Transferable items beyond the screen's row capacity remain reachable, with actual variant names. */
  R.n_items = 0;
  for (int i = 0; i < 18; i++) gc_item_add(&R, "ATTACK_TYPE_BOOSTER", i, R.party[0].b.id, 1);
  for (int i = 0; i < 11; i++) gc_item_add(&R, "BERRY", i, R.party[0].b.id, 1);
  gc_item_add(&R, "BASE_STAT_BOOSTER", STAT_ATK, R.party[0].b.id, 1); /* not transferable */
  int held[GC_MAX_ITEMS];
  const int n_held = gc_gts_held_items(&R, 0, held);
  CHECK(n_held == 29, "only transferable held items: %d", n_held);
  g_depth = n_got = 0;
  CHECK(me_flow_open(&R, chosen, NULL), "item trade opens");
  press(BTN_DOWN); press(BTN_DOWN); press(BTN_ACTION); press(BTN_ACTION);
  draw(0);
  CHECK(has_text(0, "Choose an item to trade."), "item picker");
  CHECK(n_got == 0, "does not give away the first item");
  for (int i = 0; i < ME_MAX_OPTIONS - 2; i++) press(BTN_DOWN);
  draw(1);
  CHECK(has_text(1, "Next page"), "all inventory pages available");
  press(BTN_ACTION);
  press(BTN_DOWN); /* ordinal 24 */
  press(BTN_ACTION);
  CHECK(n_got == 1 && got.option == 2 && got.b == 0 && got.c == 24, "second page sends held-item ordinal 24: %d", got.c);
  const gc_item traded = R.items[held[24]];
  bool ok = false;
  gc_rq_reset(&gc_int(&R)->rq);
  CHECK(gc_mystery_execute(&R, got.option, got.b, got.c, &why, &ok) && ok, "real item-trade handler accepts the UI choice");
  bool remains = false;
  for (int i = 0; i < R.n_items; i++) remains |= R.items[i].pokemon_id == traded.pokemon_id && R.items[i].arg == traded.arg && !strcmp(R.items[i].key, traded.key);
  CHECK(!remains, "the selected item, rather than item zero, was consumed");
  gc_run_until_screen(&R);
  CHECK(R.screen.kind == GC_SCREEN_REWARD && R.screen.n_options == 1, "upgraded item reaches the reward screen");

  /* Field Trip must transmit the original moveset slot after omitting empty rows. */
  run_at(GC_ME_FIELD_TRIP, 3);
  R.party[0].b.n_moves = 3;
  R.party[0].b.moves[0] = MOVE_ID_TACKLE;
  R.party[0].b.moves[1] = 0;
  R.party[0].b.moves[2] = MOVE_ID_SURF;
  R.screen.option_pick[0] = GC_PICK_MEMBER_MOVE;
  CHECK(me_flow_open(&R, chosen, NULL), "sparse move picker");
  press(BTN_ACTION); press(BTN_ACTION); press(BTN_DOWN); press(BTN_ACTION);
  CHECK(n_got == 1 && got.c == 2, "actual move slot retained (%d)", got.c);

  /* Markup is rendered rather than printed; long descriptions/tooltips can be read without drawing over the windows. */
  static me_args long_text;
  memset(&long_text, 0, sizeof long_text);
  snprintf(long_text.title, sizeof long_text.title, "Details");
  snprintf(long_text.description, sizeof long_text.description, "@[TOOLTIP_TITLE]{First}\nSecond\nThird\nFourth\nFifth\nLast description");
  snprintf(long_text.query, sizeof long_text.query, "What will you do?");
  snprintf(long_text.options[0].label, ME_LABEL_LEN, "Read");
  snprintf(long_text.options[0].tooltip, ME_TOOLTIP_LEN, "Tip1\nTip2\nTip3\nTip4\nTip5\nLast tip");
  long_text.options[0].ok = true;
  long_text.n = 1;
  long_text.n_intro = 1;
  snprintf(long_text.intro[0].key, ME_SPRITE_LEN, "images/mystery-encounters/field_trip_teacher");
  long_text.intro[0].x = 160; long_text.intro[0].y = 164; long_text.intro[0].scale = 1;
  g_frame.src_h = g_frame.src_w = 96;
  n_sil = 0;
  g_depth = 0;
  CHECK(me_screen_open(&long_text), "long text opens");
  draw(0);
  CHECK(n_sil == 1 && g_sil_y[0] - g_sil_scale[0] * 96 >= 122, "art fits below description and query");
  CHECK(has_text(0, "First") && !has_text(0, "@[") && !has_text(0, "Last description"), "first description page, no raw markup");
  draw(1);
  CHECK(has_text(1, "Tip1") && !has_text(1, "Last tip") && has_text(1, "Details 1/2"), "tooltip is paged");
  press(BTN_RIGHT);
  draw(0);
  CHECK(has_text(0, "Last description") && has_text(0, "What will you do?"), "last description and question reachable");
  draw(1);
  CHECK(has_text(1, "Last tip") && has_text(1, "Details 2/2"), "last tooltip reachable");
  for (int i = 0; i < n_drawn; i++) CHECK(g_drawn[i].y < 240, "text stays on the bottom screen: %g", g_drawn[i].y);
  press(BTN_LEFT);
  draw(1);
  CHECK(has_text(1, "Tip1"), "details can be reread");

  g_frame.src_h = g_frame.src_w = 0;

  /* Audit the actual English texts of all fourteen source-backed encounters, not invented short translations. */
  use_source_locales = true;
  const int ported[] = {GC_ME_DEPARTMENT_STORE_SALE, GC_ME_SHADY_VITAMIN_DEALER, GC_ME_LOST_AT_SEA, GC_ME_FIELD_TRIP,
    GC_ME_PART_TIMER, GC_ME_AN_OFFER_YOU_CANT_REFUSE, GC_ME_THE_POKEMON_SALESMAN, GC_ME_FUN_AND_GAMES,
    GC_ME_GLOBAL_TRADE_SYSTEM, GC_ME_FIGHT_OR_FLIGHT, GC_ME_SLUMBERING_SNORLAX, GC_ME_BERRIES_ABOUND,
    GC_ME_DARK_DEAL, GC_ME_TRAINING_SESSION};
  for (size_t t = 0; t < sizeof ported / sizeof *ported; t++) {
    g_depth = n_got = 0;
    gc_run_new(&R, GAME_MODES_CLASSIC, "mystery-ui-locale", 812);
    CHECK(gc_run_set_starters(&R, starters, 2, &why), "locale party");
    R.wave = 37; R.money = 1000000;
    pr_run_reset_seed(&R.run, R.wave);
    R.party[0].b.n_moves = 3;
    R.party[0].b.moves[0] = MOVE_ID_MEAN_LOOK;
    R.party[0].b.moves[1] = MOVE_ID_THIEF;
    R.party[0].b.moves[2] = MOVE_ID_CHARM;
    R.mystery_type = R.screen.mystery_type = (int8_t)ported[t];
    R.screen.kind = GC_SCREEN_MYSTERY;
    const pr_rng before = R.run.rng;
    gc_mystery_setup_screen(&R);
    CHECK(!memcmp(&before, &R.run.rng, sizeof before), "encounter %d screen preserves RNG", ported[t]);
    CHECK(me_flow_open(&R, chosen, NULL), "encounter %d opens", ported[t]);
    draw(0);
    CHECK(!has_text(0, "@[") && !has_text(0, "{{") && !has_text(0, "some money") && !has_text(0, "a Pokemon"),
          "encounter %d description carries real values", ported[t]);
    for (int i = 0; i < R.screen.n_options; i++) {
      draw(1);
      CHECK(!has_text(1, "{{") && !has_text(1, "some money") && !has_text(1, "a Pokemon"),
            "encounter %d option %d carries real values", ported[t], i);
      press(BTN_DOWN);
    }
    if (ported[t] == GC_ME_SHADY_VITAMIN_DEALER) {
      g_depth = 0;
      R.ev_head = R.ev_count = 0;
      gc_rq_reset(&gc_int(&R)->rq);
      bool applied = false;
      CHECK(gc_mystery_execute(&R, 0, 0, 0, &why, &applied) && applied, "vitamin deal applies");
      gc_event e;
      bool selected = false;
      while (gc_run_next_event(&R, &e))
        if (e.msg.key && !strcmp(e.msg.key, "mysteryEncounters/shadyVitaminDealer:option.selected")) {
          char message[1200]; be_format_message(&e.msg, message, sizeof message);
          selected = true;
          CHECK(!strstr(message, "{{") && strstr(message, "Pikachu") && strstr(message, "boosts"), "actual vitamin result: %s", message);
        }
      CHECK(selected, "vitamin rewards are announced");
      gc_rphase post = gc_rp(RP_ME_POST, GC_ME_SHADY_VITAMIN_DEALER, 0, 0);
      gc_mystery_post(&R, &post);
      bool side = false;
      while (gc_run_next_event(&R, &e))
        if (e.msg.key && !strcmp(e.msg.key, "mysteryEncounters/shadyVitaminDealer:cheapSideEffects")) {
          char message[1200]; be_format_message(&e.msg, message, sizeof message);
          side = true;
          CHECK(!strstr(message, "{{") && strstr(message, "Pikachu"), "actual side effect names the Pokemon: %s", message);
        }
      CHECK(side, "vitamin side effects announced");
    }
    if (ported[t] == GC_ME_FIELD_TRIP) {
      g_depth = n_got = 0;
      CHECK(me_flow_open(&R, chosen, NULL), "actual Field Trip dialogue");
      press(BTN_ACTION); press(BTN_ACTION); /* Mean Look is status, wrong for physical */
      press(BTN_ACTION);
      CHECK(n_got == 1 && strstr(got.selected, "isn’t a Physical move") && !strstr(got.selected, "{{"), "wrong-category explanation: %s", got.selected);
      g_depth = n_got = 0;
      CHECK(me_flow_open(&R, chosen, NULL), "correct Field Trip dialogue");
      press(BTN_DOWN); press(BTN_DOWN); press(BTN_ACTION); press(BTN_ACTION); press(BTN_ACTION);
      CHECK(n_got == 1 && strstr(got.selected, "Thank you so much") && strstr(got.selected, "valuable experience"), "correct-category reward explanation: %s", got.selected);
    }
    if (ported[t] == GC_ME_THE_POKEMON_SALESMAN) {
      for (int shiny = 0; shiny < 2; shiny++) {
        for (int a = 0; a < R.screen.n_me_args; a++)
          if (!strcmp(R.screen.me_args[a].name, "salesmanShiny")) R.screen.me_args[a].v = (uint32_t)shiny;
        g_depth = n_got = 0;
        CHECK(me_flow_open(&R, chosen, NULL), "Salesman variant %d", shiny);
        draw(0);
        CHECK(has_text(0, shiny ? "has a pigment" : "carries an ability"), "correct Salesman description variant");
        draw(1);
        CHECK(has_text(1, shiny ? "Gain a shiny" : "Hidden Ability"), "correct Salesman offer tooltip");
      }
    }
    if (ported[t] == GC_ME_SLUMBERING_SNORLAX) {
      R.party[0].b.n_moves = 1;
      R.party[0].b.moves[0] = MOVE_ID_TACKLE;
      gc_mystery_setup_screen(&R);
      g_depth = n_got = 0;
      CHECK(me_flow_open(&R, chosen, NULL), "disabled steal");
      press(BTN_DOWN); press(BTN_DOWN);
      draw(1);
      CHECK(has_text(1, "need to know certain moves"), "source's disabledTooltip is used");
    }
    if (ported[t] == GC_ME_DARK_DEAL) {
      R.ev_head = R.ev_count = 0;
      gc_rq_reset(&gc_int(&R)->rq);
      bool applied = false;
      CHECK(gc_mystery_execute(&R, 0, 0, 0, &why, &applied) && applied, "Dark Deal applies");
      gc_event e;
      int announced = 0;
      while (gc_run_next_event(&R, &e))
        if (e.msg.key && (!strcmp(e.msg.key, "mysteryEncounters/darkDeal:option.1.selectedDialogue") ||
                         !strcmp(e.msg.key, "mysteryEncounters/darkDeal:option.1.selectedMessage"))) {
          char message[1200]; be_format_message(&e.msg, message, sizeof message);
          announced++;
          CHECK(!strstr(message, "{{") && (strstr(message, "Pikachu") || strstr(message, "Lapras")), "actual lost Pokemon is named: %s", message);
        }
      CHECK(announced == 2, "both Dark Deal messages delivered before the battle");
    }
  }
  const pr_msg_arg berry_name = gc_m_item_arg("itemName", "BERRY", BERRY_TYPE_LUM);
  CHECK(berry_name.kind == PR_ARG_STR && !strcmp(berry_name.s, "Lum Berry"), "concrete generated item, rather than missing BERRY.name");
  const pr_msg_arg booster_name = gc_m_item_arg("itemName", "BASE_STAT_BOOSTER", STAT_ATK);
  CHECK(booster_name.kind == PR_ARG_STR && !strcmp(booster_name.s, "Protein"), "concrete vitamin name");
  const pr_msg_arg ball_name = gc_m_item_arg("itemName", "POKEBALL", -1);
  CHECK(ball_name.kind == PR_ARG_STR && !strcmp(ball_name.s, "5× Poké Ball"), "ball-pack count and name, not a nonexistent generic locale key");
  /* A long run sees more than 48 distinct types; every registered plain item must retain its locale key. */
  const pr_msg_arg first_name = gc_m_item("itemName", "SHINY_CHARM");
  int plain_types = 0;
  for (int i = 0; i < PR_ITEM_COUNT_; i++) {
    if (pr_items[i].gen) continue;
    char expected[96];
    snprintf(expected, sizeof expected, "modifierType:ModifierType.%s.name", pr_item_keys[i]);
    const pr_msg_arg named = gc_m_item("itemName", pr_item_keys[i]);
    CHECK(named.kind == PR_ARG_KEY && !strcmp(named.s, expected), "plain item %s retains its translation after many types", pr_item_keys[i]);
    plain_types++;
  }
  CHECK(plain_types > 48, "full plain-item catalog was checked: %d", plain_types);
  CHECK(!strcmp(first_name.s, "modifierType:ModifierType.SHINY_CHARM.name"), "earlier message arguments remain valid");

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
