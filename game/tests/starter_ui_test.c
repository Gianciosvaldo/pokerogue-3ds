/* starter_ui_test: the starter select screen as the player meets it (menus/starter.c + starter_model.c, hosted by
 * source/starter_host.c, fed by source/starter_data.c) over real game-core, on the host. The drawing layer is faked (no pixels): the
 * fakes record what is drawn and played, and the typing message and the Yes / No box follow menus/mui.c's rules (mui.c itself needs
 * libjansson and the renderer).
 *
 *   build/starter_ui_test [fuzz sessions, default 200]
 *
 * Presses buttons through the screen's own input function, as the app's UI does, and checks: the options menu, adding and removing
 * team members, the cost shown, Start asking "Begin with these Pokemon?" and answering Yes / No, Exit asking and answering, a team
 * game-core refuses (the box says so and the screen goes on), that the team handed over starts a real run, and under random
 * buttons on random system data that nothing breaks and that every team handed over is one gc_run_set_starters takes.
 * Exit 1 on a failed check. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"
#include "gc/run.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "starter_data.h"
#include "starter_host.h"
#include "ui.h"

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

/* ------------------------------------------------------------------ fakes: what is drawn and played */

#define LOG_MAX 65536
static char g_drawn[LOG_MAX]; /* every text and image key drawn since the last reset, one per line */
static char g_sfx[LOG_MAX];   /* every sound played since the last reset, one per line */

static void log_add(char* log, const char* s) {
  const size_t n = strlen(log), k = strlen(s);
  if (n + k + 2 >= LOG_MAX) return;
  memcpy(log + n, s, k);
  log[n + k] = '\n';
  log[n + k + 1] = 0;
}
static char g_msg_text[600];  /* what the message box showed last, line breaks and all */
static void reset_logs(void) { g_drawn[0] = g_sfx[0] = g_msg_text[0] = 0; }
static bool drawn(const char* s) { return strstr(g_drawn, s) != NULL; }
static int count_in(const char* log, const char* s) {
  int c = 0;
  for (const char* p = log; (p = strstr(p, s)); p += strlen(s)) c++;
  return c;
}

const rr_draw_opts RR_DEFAULT_OPTS = {0, 0, 1, 1, 0, RR_RGBA(255, 255, 255, 255)};
static const rr_frame g_frame;
const rr_frame* mui_frame(const char* key, const char* frame) {
  (void)key, (void)frame;
  return &g_frame;
}
bool mui_image(const char* key, const char* frame, float x, float y, float ox, float oy) {
  (void)x, (void)y, (void)ox, (void)oy;
  char t[160];
  snprintf(t, sizeof t, "image %s %s", key, frame ? frame : "");
  log_add(g_drawn, t);
  return true;
}
bool mui_image_ex(const char* key, const char* frame, float x, float y, const rr_draw_opts* o) {
  (void)o;
  return mui_image(key, frame, x, y, 0, 0);
}
void mui_clip(float x, float y, float w, float h) { (void)x, (void)y, (void)w, (void)h; }
void mui_clip_off(void) {}
void mui_window(float x, float y, float w, float h) { (void)x, (void)y, (void)w, (void)h; }
int mui_text(mui_style s, float x, float y, const char* utf8) {
  (void)s, (void)x, (void)y;
  log_add(g_drawn, utf8);
  return (int)strlen(utf8) * 6;
}
int mui_text_o(mui_style s, float x, float y, float ox, float oy, const char* utf8) {
  (void)ox, (void)oy;
  return mui_text(s, x, y, utf8);
}
int mui_text_c(mui_style s, float x, float y, const char* utf8, rr_color c, rr_color shadow) {
  (void)c, (void)shadow;
  return mui_text(s, x, y, utf8);
}
int mui_text_width(mui_style s, const char* utf8) {
  (void)s;
  return (int)strlen(utf8) * 6;
}
float mui_text_height(mui_style s, const char* utf8) {
  (void)s, (void)utf8;
  return 14;
}
/* Phaser's basicWordWrap as mui.c does it, with the 6 px a character has in this fake */
int mui_wrap(mui_style s, const char* in, float width, char* out, size_t n) {
  (void)s;
  size_t o = 0;
  int lines = 1;
  float left = width;
  bool first = true;
  const char* p = in;
  while (*p) {
    const char* sp = p;
    while (*sp && *sp != ' ') sp++;
    const size_t wl = (size_t)(sp - p);
    const float ww = (float)wl * 6.0f + (*sp ? 6.0f : 0.0f);
    if (ww > left && !first) {
      if (o + 1 < n) out[o++] = '\n';
      lines++;
      left = width;
    }
    for (size_t i = 0; i < wl && o + 1 < n; i++) out[o++] = p[i];
    left -= ww;
    first = false;
    if (*sp) {
      if (o + 1 < n) out[o++] = ' ';
      p = sp + 1;
    } else {
      p = sp;
    }
  }
  out[o] = 0;
  return lines;
}
rr_color mui_style_color(mui_style s, bool shadow) {
  (void)s, (void)shadow;
  return 0;
}
const char* mui_t(const char* key) { return key; }
void mui_sfx(const char* key) { log_add(g_sfx, key); }
void rr_draw(const rr_frame* f, float x, float y, const rr_draw_opts* o) { (void)f, (void)x, (void)y, (void)o; }
void rr_nineslice(const rr_frame* f, float x, float y, float w, float h, int border) { (void)f, (void)x, (void)y, (void)w, (void)h, (void)border; }
void rr_rect(float x, float y, float w, float h, rr_color c) { (void)x, (void)y, (void)w, (void)h, (void)c; }

/* sprites and icons: the screen asks for them, nothing is loaded */
bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back) {
  (void)s, (void)m, (void)back;
  return false;
}
void menu_sprite_free(menu_sprite* s) { memset(s, 0, sizeof *s); }
const rr_frame* menu_sprite_frame(const menu_sprite* s, uint32_t ms) {
  (void)s, (void)ms;
  return NULL;
}
void menu_draw_icon_tinted(const menu_mon* m, float x, float y, float ox, float oy, float scale, rr_color tint) {
  (void)x, (void)y, (void)ox, (void)oy, (void)scale, (void)tint;
  char t[64];
  snprintf(t, sizeof t, "icon %u", m->species);
  log_add(g_drawn, t);
}

/* the typing message and the Yes / No box: menus/mui.c's rules */
static void msg_done(mui_msg* m) {
  if (m->prompt) {
    m->pending_prompt = false;
    m->awaiting_action = true;
    return;
  }
  mui_cb cb = m->cb;
  m->cb = NULL;
  if (cb) cb(m->ctx);
}
void mui_msg_show(mui_msg* m, const char* text, int delay_ms, mui_cb cb, void* ctx, bool prompt) {
  snprintf(m->text, sizeof m->text, "%s", text);
  m->delay_ms = delay_ms;
  m->cb = cb;
  m->ctx = ctx;
  m->prompt = prompt;
  m->awaiting_action = false;
  m->acc = 0;
  if (delay_ms > 0) {
    m->shown = 0;
    m->pending_prompt = prompt;
  } else {
    m->shown = (int)strlen(m->text);
    m->pending_prompt = false;
    msg_done(m);
  }
}
void mui_msg_update(mui_msg* m, float dt_ms) {
  int len = (int)strlen(m->text);
  if (m->shown >= len || m->delay_ms <= 0) return;
  m->acc += dt_ms;
  while (m->acc >= m->delay_ms && m->shown < len) {
    m->acc -= m->delay_ms;
    m->shown++;
    while (m->shown < len && ((unsigned char)m->text[m->shown] & 0xC0) == 0x80) m->shown++;
  }
  if (m->shown >= len) msg_done(m);
}
bool mui_msg_busy(const mui_msg* m) { return m->pending_prompt || m->awaiting_action || (m->delay_ms > 0 && m->shown < (int)strlen(m->text)); }
bool mui_msg_input(mui_msg* m, mui_button b) {
  if (m->pending_prompt) return true;
  if (!m->awaiting_action) return false;
  if (b != MB_ACTION && b != MB_CANCEL) return true;
  mui_sfx("ui/select");
  m->awaiting_action = false;
  mui_cb cb = m->cb;
  m->cb = NULL;
  if (cb) cb(m->ctx);
  return true;
}
int mui_msg_lines(const mui_msg* m) {
  int n = 1;
  for (const char* p = m->text; *p; p++) n += *p == '\n';
  return n;
}
void mui_msg_draw(const mui_msg* m, mui_style s, float x, float y) {
  (void)s, (void)x, (void)y;
  char t[600];
  snprintf(g_msg_text, sizeof g_msg_text, "%.*s", m->shown, m->text);
  snprintf(t, sizeof t, "message %.*s", m->shown, m->text);
  /* a wrapped message is one entry in the log: the wrap leaves the space before each line break, so a break is a space */
  for (char *r = t, *w = t;; r++) {
    if (*r == '\n') {
      if (w > t && w[-1] == ' ') continue;
      *w++ = ' ';
    } else if (!(*w++ = *r)) {
      break;
    }
  }
  log_add(g_drawn, t);
}
void mui_confirm_show(mui_confirm* c, mui_cb yes, mui_cb no, void* ctx) {
  c->active = true;
  c->cursor = 0;
  c->yes = yes;
  c->no = no;
  c->ctx = ctx;
}
bool mui_confirm_input(mui_confirm* c, mui_button b) {
  if (!c->active) return false;
  switch (b) {
    case MB_UP:
    case MB_DOWN:
      c->cursor ^= 1;
      mui_sfx("ui/select");
      return true;
    case MB_ACTION:
    case MB_CANCEL: {
      const bool yes = b == MB_ACTION && c->cursor == 0;
      c->active = false;
      mui_sfx("ui/select");
      mui_cb cb = yes ? c->yes : c->no;
      if (cb) cb(c->ctx);
      return true;
    }
    default:
      return true;
  }
}
void mui_confirm_draw(const mui_confirm* c) {
  if (c->active) log_add(g_drawn, "confirm box");
}

/* ------------------------------------------------------------------ fakes: the UI stack and strings */

static ui_screen* g_screen;
void ui_set(ui_screen* s) {
  if (g_screen && g_screen->leave) g_screen->leave(g_screen);
  g_screen = s;
  if (s->enter) s->enter(s);
}
bool ui_space(ui_space_id sp) {
  (void)sp;
  return true;
}
float ui_now(void) { return 1234.5f; }
uint32_t ui_frame(void) { return 77; }
void mui_tick(float dt) { (void)dt; }
void mui_set_origin(rr_screen s, int px, int py) { (void)s, (void)px, (void)py; }

const pr_species* menu_species(uint16_t id);
const char* i18n_species_name(int id) {
  const pr_species* sp = menu_species((uint16_t)id);
  return sp ? sp->name : "?";
}
const char* i18n_t(char* out, int n, const char* key, ...) {
  snprintf(out, (size_t)n, "%s", key);
  return out;
}

/* ------------------------------------------------------------------ helpers */

static uint64_t rng_state = 0x2545F4914F6CDD1Dull;
static uint32_t rnd(uint32_t n) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 7;
  rng_state ^= rng_state << 17;
  return n ? (uint32_t)((rng_state >> 11) % n) : 0;
}

static gc_system SYS;
static gc_run RUN;

/* what the host reported */
static struct {
  int started, exited;
  gc_starter team[6];
  int n_team;
} R;
static void on_start(const gc_starter* team, int n, void* arg) {
  (void)arg;
  R.started++;
  R.n_team = n;
  memcpy(R.team, team, (size_t)n * sizeof team[0]);
}
static void on_exit_cb(void* arg) {
  (void)arg;
  R.exited++;
}

static bool open_screen(const char* notice) {
  memset(&R, 0, sizeof R);
  g_screen = NULL;
  reset_logs();
  const sh_events ev = {on_start, on_exit_cb, NULL};
  return sh_open(&SYS, notice, &ev);
}
static void press(mui_button b) { g_screen->input(g_screen, (ui_button)b); }
static void tick(float ms) {
  for (float t = 0; t < ms; t += 16.7f) g_screen->update(g_screen, 16.7f);
}
static void draw(void) {
  g_drawn[0] = 0;
  g_screen->draw(g_screen, RR_BOTTOM);
  g_screen->draw(g_screen, RR_TOP);
}

static bool core_takes(const gc_starter* team, int n, const char** why) {
  gc_run_new(&RUN, GAME_MODES_CLASSIC, "starteruitestseed0000001", 7);
  RUN.sys = &SYS;
  return gc_run_set_starters(&RUN, team, n, why);
}

static double team_value(const gc_starter* team, int n) {
  double v = 0;
  for (int i = 0; i < n; i++) v += gc_starter_value(&SYS, team[i].species);
  return v;
}

static void random_system(int catches) {
  gc_system_init_defaults(&SYS);
  for (int i = 0; i < catches; i++) {
    const pr_species* sp = &pr_species_table[rnd(PR_SPECIES_COUNT)];
    gc_mon m;
    memset(&m, 0, sizeof m);
    m.b.species = (uint16_t)sp->id;
    m.b.gender = sp->male_permille < 0 ? -1 : (int8_t)rnd(2);
    m.b.shiny = rnd(4) == 0;
    m.ability_index = (uint8_t)rnd(2);
    m.b.nature = (uint8_t)rnd(25);
    for (int k = 0; k < 6; k++) m.b.ivs[k] = (uint8_t)rnd(32);
    gc_system_set_pokemon_caught(&SYS, &m, true, false);
  }
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    if (SYS.dex[id].caught_attr && rnd(3) == 0) SYS.starters[id].value_reduction = rnd(3);
  }
}

/* ------------------------------------------------------------------ scenarios */

/* the way through a new save: pick two species, change one's mind, begin */
static void test_happy_path(void) {
  gc_system_init_defaults(&SYS);
  CHECK(open_screen(NULL), "the screen opens on a new save");
  CHECK(g_screen && !strcmp(g_screen->name, "starter-select"), "it is on the stack");
  draw();
  CHECK(drawn("0/10"), "an empty team costs 0/10");
  CHECK(drawn("starterSelectUiHandler:randomize") && drawn("common:start"), "Randomize and Start are labelled");

  /* A on the first species: the options menu */
  press(MB_ACTION);
  draw();
  CHECK(drawn("starterSelectUiHandler:addToParty") && drawn("menu:cancel"), "the options menu offers Add to Party and Cancel");
  CHECK(count_in(g_sfx, "ui/select") >= 1, "a sound");
  reset_logs();
  press(MB_ACTION); /* Add to Party */
  CHECK(count_in(g_sfx, "cry/") == 1 && count_in(g_sfx, "ui/select") == 1, "adding plays the cry and the select sound: %s", g_sfx);
  draw();
  static sm_entry list[SS_MAX_ENTRIES];
  ss_list(&SYS, list, SS_MAX_ENTRIES, NULL);
  char want[32];
  snprintf(want, sizeof want, "%d/10", (int)list[0].cost);
  CHECK(drawn(want), "after one species the cost is %s", want);

  /* the same species again: the menu offers Remove, and Cancel leaves it */
  press(MB_ACTION);
  draw();
  CHECK(drawn("starterSelectUiHandler:removeFromParty") && !drawn("starterSelectUiHandler:addToParty"), "a member of the team offers Remove");
  press(MB_CANCEL); /* B picks the last option, Cancel */
  draw();
  CHECK(!drawn("starterSelectUiHandler:removeFromParty"), "B closes the options menu");
  CHECK(drawn(want), "and the team is as it was");

  /* a second species */
  press(MB_RIGHT);
  press(MB_ACTION);
  press(MB_ACTION);
  draw();
  const double both = list[0].cost + list[1].cost;
  snprintf(want, sizeof want, "%g/10", both);
  CHECK(drawn(want), "two species: %s", want);

  /* Start, answered No */
  reset_logs();
  press(MB_MENU);
  CHECK(count_in(g_sfx, "ui/select") == 1, "Start plays the select sound");
  tick(2500); /* the question types out */
  draw();
  CHECK(drawn("starterSelectUiHandler:confirmStartTeam") && drawn("confirm box"), "\"Begin with these Pokemon?\" with Yes / No");
  press(MB_CANCEL);
  draw();
  CHECK(!drawn("confirm box") && !drawn("starterSelectUiHandler:confirmStartTeam"), "No closes the question");
  CHECK(R.started == 0, "nothing started");

  /* Start, answered Yes */
  press(MB_MENU);
  tick(2500);
  press(MB_ACTION);
  CHECK(R.started == 1 && R.n_team == 2, "Yes: the team of %d is handed over (%d times)", R.n_team, R.started);
  CHECK(R.team[0].species == list[0].species && R.team[1].species == list[1].species, "in the order picked");
  const char* why = NULL;
  CHECK(core_takes(R.team, R.n_team, &why), "game-core takes it: %s", why ? why : "");
  CHECK(RUN.n_party == 2 && RUN.party[0].b.species == list[0].species && RUN.party[1].b.species == list[1].species, "the run's party is the team");
  CHECK(RUN.party[0].b.nature == R.team[0].nature && RUN.party[0].b.ivs[0] == 15, "its nature and IVs are the dex's");

  /* after it, the screen takes nothing more */
  press(MB_ACTION);
  press(MB_MENU);
  tick(2500);
  press(MB_ACTION);
  CHECK(R.started == 1 && R.exited == 0, "the screen is finished: %d starts, %d exits", R.started, R.exited);
}

static void test_exit(void) {
  gc_system_init_defaults(&SYS);
  CHECK(open_screen(NULL), "open");
  press(MB_CANCEL); /* B on the grid */
  tick(2500);
  draw();
  CHECK(drawn("starterSelectUiHandler:confirmExit") && drawn("confirm box"), "\"Do you want to exit?\"");
  press(MB_CANCEL); /* No */
  CHECK(R.exited == 0, "No: stays");
  draw();
  CHECK(!drawn("confirm box"), "the box is gone");
  press(MB_CANCEL);
  tick(2500);
  press(MB_DOWN); /* the cursor moves to No */
  press(MB_UP);   /* and back to Yes */
  press(MB_ACTION);
  CHECK(R.exited == 1 && R.started == 0, "Yes: the screen asked to exit once");
  /* Start with nobody in the team is refused with the error sound */
  CHECK(open_screen(NULL), "open again");
  press(MB_MENU);
  CHECK(count_in(g_sfx, "ui/error") == 1 && count_in(g_sfx, "ui/select") == 0, "an empty team cannot begin");
  tick(2500);
  draw();
  CHECK(!drawn("starterSelectUiHandler:confirmStartTeam"), "and no question is asked");
}

static void test_remove(void) {
  gc_system_init_defaults(&SYS);
  CHECK(open_screen(NULL), "open");
  press(MB_ACTION);
  press(MB_ACTION); /* add the first */
  press(MB_ACTION);
  draw();
  CHECK(drawn("starterSelectUiHandler:removeFromParty"), "Remove is offered");
  press(MB_ACTION); /* Remove from Party */
  draw();
  CHECK(drawn("0/10"), "the team is empty again");
  press(MB_MENU);
  tick(2500);
  draw();
  CHECK(!drawn("confirm box"), "an emptied team cannot begin");
}

/* game-core refuses what the dex no longer allows: the box says so, the screen goes on, and the player can fix it */
static void test_refused(void) {
  gc_system_init_defaults(&SYS);
  CHECK(open_screen(NULL), "open");
  static sm_entry list[SS_MAX_ENTRIES];
  ss_list(&SYS, list, SS_MAX_ENTRIES, NULL);
  const uint16_t sp = list[0].species;
  press(MB_ACTION);
  press(MB_ACTION);
  const uint32_t natures = SYS.dex[sp].nature_attr;
  SYS.dex[sp].nature_attr = 0; /* the dex lost its nature after the screen was listed */
  press(MB_MENU);
  tick(2500);
  press(MB_ACTION);
  CHECK(R.started == 0, "game-core's refusal stops the start");
  tick(2500);
  draw();
  CHECK(drawn("message starterSelectUiHandler:invalidParty (starterSelectUiHandler:lockedNature)"), "the box says why");
  press(MB_ACTION); /* A closes the message */
  draw();
  CHECK(!drawn("message"), "the message closes");
  SYS.dex[sp].nature_attr = natures;
  press(MB_MENU);
  tick(2500);
  press(MB_ACTION);
  CHECK(R.started == 1 && R.n_team == 1 && R.team[0].species == sp, "after the fix the same team begins");
}

/* a notice for the player (an unreadable system file) waits for A */
static void test_notice(void) {
  gc_system_init_defaults(&SYS);
  CHECK(open_screen("The system data is damaged."), "open with a notice");
  tick(2500);
  draw();
  CHECK(drawn("message The system data is damaged."), "the notice is in the box");
  press(MB_RIGHT);
  press(MB_MENU);
  draw();
  CHECK(drawn("message The system data is damaged."), "buttons do not reach the screen under it");
  CHECK(!drawn("confirm box"), "Start does not ask anything under it");
  press(MB_ACTION);
  draw();
  CHECK(!drawn("message"), "A closes it");
  press(MB_ACTION);
  draw();
  CHECK(drawn("starterSelectUiHandler:addToParty") || drawn("starterSelectUiHandler:removeFromParty"), "then the screen answers");
}

/* a long notice is wrapped to the box (the message box does not wrap by itself): no line is wider than the box, nothing is lost,
 * and it still waits for A */
static void test_long_notice(void) {
  static const char text[] =
      "The system data is damaged (it failed its checksum). It is left as it is, and nothing is saved to it until it is removed.";
  gc_system_init_defaults(&SYS);
  CHECK(open_screen(text), "open with a long notice");
  tick(5000);
  draw();
  CHECK(g_msg_text[0] != 0, "the notice is in the box");
  char shown[600];
  snprintf(shown, sizeof shown, "%s", g_msg_text);
  CHECK(strchr(shown, '\n') != NULL, "a notice of %zu characters takes more than one line", strlen(text));
  int widest = 0, len = 0, lines = 1;
  char joined[600];
  size_t j = 0;
  for (const char* q = shown; *q; q++) {
    if (*q == '\n') {
      lines++;
      if (len > widest) widest = len;
      len = 0;
      if (j == 0 || joined[j - 1] != ' ') joined[j++] = ' '; /* the wrap keeps the space before a break */
    } else {
      len++;
      joined[j++] = *q;
    }
  }
  if (len > widest) widest = len;
  joined[j] = 0;
  CHECK(widest * 6 <= MUI_W - 2 - 16, "no line is wider than the box (%d characters)", widest);
  CHECK(lines <= 3, "it takes %d lines", lines);
  CHECK(!strcmp(joined, text), "nothing is lost in the wrap");
  press(MB_ACTION);
  draw();
  CHECK(!drawn("message"), "A closes it");
}

/* everything unlocked: fill the budget; the screen never lets the cost go over it */
static void test_budget(void) {
  gc_system_init_defaults(&SYS);
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    SYS.dex[id].caught_attr = gc_species_full_unlocks_data(id);
    SYS.dex[id].nature_attr = 0x3FFFFFEu;
    SYS.starters[id].ability_attr = 7;
  }
  CHECK(open_screen(NULL), "open");
  static sm_entry list[SS_MAX_ENTRIES];
  const int n = ss_list(&SYS, list, SS_MAX_ENTRIES, NULL);
  CHECK(n == gc_starter_cost_count, "%d species", n);
  for (int rounds = 0; rounds < n; rounds++) { /* A, A on every species in turn */
    press(MB_ACTION);
    press(MB_ACTION);
    press(MB_RIGHT);
  }
  press(MB_MENU);
  tick(2500);
  press(MB_ACTION);
  CHECK(R.started == 1, "the team begins");
  CHECK(R.n_team >= 1 && R.n_team <= 6, "%d members", R.n_team);
  CHECK(team_value(R.team, R.n_team) <= 10.0 + 1e-9, "its cost %g is within the budget", team_value(R.team, R.n_team));
  const char* why = NULL;
  CHECK(core_takes(R.team, R.n_team, &why), "game-core takes it: %s", why ? why : "");
}

/* random buttons on random systems: nothing breaks, and every team handed over starts a run */
static void test_fuzz(int sessions) {
  long presses = 0, teams = 0, exits = 0;
  for (int s = 0; s < sessions; s++) {
    random_system((int)rnd(300));
    if (!open_screen(NULL)) continue; /* a system with nothing to list: cannot happen with the defaults, but is not an error */
    for (int i = 0; i < 600; i++) {
      const uint32_t r = rnd(100);
      if (r < 70)
        press((mui_button)rnd(9)); /* UP DOWN LEFT RIGHT SUBMIT ACTION CANCEL MENU STATS */
      else if (r < 90)
        press(MB_ACTION);
      else
        tick((float)rnd(3000));
      presses++;
      if (rnd(40) == 0) draw();
      if (R.started) {
        const char* why = NULL;
        teams++;
        CHECK(R.n_team >= 1 && R.n_team <= 6, "team size %d", R.n_team);
        CHECK(team_value(R.team, R.n_team) <= 10.0 + 1e-9, "cost %g over the budget", team_value(R.team, R.n_team));
        CHECK(core_takes(R.team, R.n_team, &why), "game-core refused a team the screen handed over: %s", why ? why : "?");
        open_screen(NULL);
      }
      if (R.exited) {
        exits++;
        open_screen(NULL);
      }
    }
  }
  printf("  %d sessions, %ld presses, %ld teams handed over (all taken by game-core), %ld exits\n", sessions, presses, teams, exits);
  CHECK(teams > 0, "the fuzz must reach Start sometimes");
}

int main(int argc, char** argv) {
  const int sessions = argc > 1 ? atoi(argv[1]) : 200;
  test_happy_path();
  test_exit();
  test_remove();
  test_refused();
  test_notice();
  test_long_notice();
  test_budget();
  test_fuzz(sessions);
  printf("%d checks, %d failed\n", checks, failed);
  printf(failed ? "FAILED\n" : "OK\n");
  return failed ? 1 : 0;
}
