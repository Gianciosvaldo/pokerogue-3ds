/*
 * startertest: the starter select screen's input rules (menus/starter_model.c) pressed button by button. No drawing, no game-core:
 * the model is plain C, so a list of species and costs is enough.
 *
 * What the expectations come from: upstream's StarterSelectUiHandler read at the pinned commit (the line numbers in
 * starter_model.c). The arithmetic of calcStarterIconY and findClosestStarterIndex was also run in Node 22 (the tables below). The
 * handler itself needs Phaser and the whole scene, so there is no differential against the running upstream handler.
 *
 *   make -C game/tests startertest && game/tests/build/startertest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "starter_model.h"

static int g_checks, g_fail;

#define CHECK(cond, ...)                                                    \
  do {                                                                      \
    g_checks++;                                                             \
    if (!(cond)) {                                                          \
      g_fail++;                                                             \
      printf("FAIL %s:%d: %s -- ", __FILE__, __LINE__, #cond);              \
      printf(__VA_ARGS__);                                                  \
      printf("\n");                                                         \
    }                                                                       \
  } while (0)

/* a list of n species 1..n at the given costs (cycling through `costs`) */
static sm_entry g_list[1100];
static void make_list(int n, const double* costs, int ncosts) {
  memset(g_list, 0, sizeof g_list);
  for (int i = 0; i < n; i++) {
    g_list[i].species = (uint16_t)(i + 1);
    g_list[i].cost = g_list[i].base_cost = costs[i % ncosts];
  }
}

static int g_rnd_next;
static int rnd_fixed(void* ctx, int n) {
  (void)ctx;
  return g_rnd_next % n;
}

static sm_model M;
static void start(int n, const double* costs, int ncosts, int budget) {
  make_list(n, costs, ncosts);
  g_rnd_next = 0;
  sm_init(&M, g_list, n, budget, rnd_fixed, NULL);
}
static sm_result press(mui_button b) { return sm_input(&M, b); }
static void presses(mui_button b, int times) {
  for (int i = 0; i < times; i++) press(b);
}
/* the grid cursor as an absolute index into the list */
static int at(void) { return M.scroll * SM_COLS + M.cursor; }

static const double ONES[] = {1};

/* ---------------------------------------------------------------- upstream's arithmetic */

static void test_icon_positions(void) {
  /* node: Math.round(38 + 107/7/2 + 107/7 * i) */
  static const int want[6] = {46, 61, 76, 92, 107, 122};
  for (int i = 0; i < 6; i++) CHECK(sm_party_icon_y(i) == want[i], "icon %d at %d", i, sm_party_icon_y(i));
}

/* node: findClosestStarterIndex(13 + row * 17, team) for team 1..6 and grid rows 0..6 */
static void test_closest_party_member(void) {
  static const char* const want[6] = {"0,0,0,0,0,0,0", "0,0,1,1,1,1,1", "0,0,1,2,2,2,2",
                                      "0,0,1,2,3,3,3", "0,0,1,2,3,4,4", "0,0,1,2,3,4,5"};
  static const double costs[] = {1};
  for (int team = 1; team <= 6; team++) {
    char got[64] = "";
    for (int row = 0; row < 7; row++) {
      start(100, costs, 1, 10);
      for (int t = 0; t < team; t++) sm_add(&M, (uint16_t)(t + 1));
      M.cursor = row * SM_COLS; /* column 0, so LEFT leaves the grid */
      sm_result r = press(MB_LEFT);
      if (row == 0) {
        CHECK(r == SM_OK && M.focus == SM_RANDOM, "row 0 goes to Randomize");
        strcat(got, row ? "," : "");
        strcat(got, "0");
        continue;
      }
      CHECK(r == SM_OK && M.focus == SM_PARTY, "team %d row %d focus %d", team, row, M.focus);
      char one[8];
      snprintf(one, sizeof one, "%s%d", row ? "," : "", M.party_cursor);
      strcat(got, one);
    }
    /* row 0 is the Randomize button, whose party index upstream never reads: the table's first entry is 0 for every team */
    CHECK(!strcmp(got, want[team - 1]), "team %d: %s (want %s)", team, got, want[team - 1]);
  }
}

static void test_value_text(void) {
  char t[32];
  static const struct { double v; const char* s; } cases[] = {{0.5, ".5"}, {3, "3"}, {4.5, "4.5"}, {10, "10"}, {0.25, ".25"}, {3.25, "3.25"}, {0.75, ".75"}, {0, "0"}};
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    sm_cost_text(t, sizeof t, cases[i].v);
    CHECK(!strcmp(t, cases[i].s), "%g -> %s (want %s)", cases[i].v, t, cases[i].s);
  }
  sm_value_text(t, sizeof t, 4.5, 10);
  CHECK(!strcmp(t, "4.5/10"), "%s", t);
  sm_value_text(t, sizeof t, 0, 10);
  CHECK(!strcmp(t, "0/10"), "%s", t);
  sm_value_text(t, sizeof t, 0.5, 10);
  CHECK(!strcmp(t, ".5/10"), "%s", t);
  sm_entry e = {.species = 1, .cost = 3, .base_cost = 3};
  CHECK(sm_cost_style(&e) == 0, "unchanged");
  e.cost = 2;
  CHECK(sm_cost_style(&e) == 1, "reduced by one");
  e.cost = 2.5;
  e.base_cost = 3;
  CHECK(sm_cost_style(&e) == 1, "reduced by a half");
  e.cost = 1;
  CHECK(sm_cost_style(&e) == 2, "reduced by two");
}

/* ---------------------------------------------------------------- the grid */

static void test_grid_moves(void) {
  start(30, ONES, 1, 10); /* 4 rows: 9, 9, 9, 3 */
  CHECK(M.focus == SM_GRID && at() == 0, "starts on the first species");
  CHECK(sm_species(&M) == 1, "panel shows species 1");
  CHECK(press(MB_RIGHT) == SM_OK && at() == 1, "right");
  CHECK(press(MB_DOWN) == SM_OK && at() == 10, "down");
  CHECK(press(MB_LEFT) == SM_OK && at() == 9, "left");
  /* left at column 0 of a row below the first, with no team: wraps to the end of the row */
  CHECK(press(MB_LEFT) == SM_OK && at() == 17, "left wraps within the row (%d)", at());
  CHECK(press(MB_RIGHT) == SM_OK && at() == 9, "right wraps within the row (%d)", at());
  /* the last row has 3 species: right of the third wraps to its first */
  M.cursor = 27;
  CHECK(press(MB_RIGHT) == SM_OK && at() == 28 && press(MB_RIGHT) == SM_OK && at() == 29, "right in the last row");
  CHECK(press(MB_RIGHT) == SM_OK && at() == 27, "right at the end of the last row wraps (%d)", at());
  /* down from the last row wraps to the first row of the same column */
  M.cursor = 28;
  CHECK(press(MB_DOWN) == SM_OK && at() == 1, "down from the last row wraps (%d)", at());
  /* down from a row whose cell below does not exist (column 5 of row 2: 18+5 = 23, below it 32 > 29) wraps too */
  M.cursor = 23;
  CHECK(press(MB_DOWN) == SM_OK && at() == 5, "no cell below: wraps to the first row (%d)", at());
  /* up from the first row wraps to the last row (no filter bar), the column above when the last row is short */
  M.cursor = 1;
  CHECK(press(MB_UP) == SM_OK && at() == 28, "up from the first row (%d)", at());
  M.cursor = 5;
  CHECK(press(MB_UP) == SM_OK && at() == 23, "up into a short last row takes the row above (%d)", at());
  CHECK(press(MB_UP) == SM_OK && at() == 14, "up (%d)", at());
}

static void test_single_row(void) {
  start(5, ONES, 1, 10);
  CHECK(press(MB_DOWN) == SM_IGNORED && at() == 0, "down in a single row does nothing without a filter bar");
  CHECK(press(MB_UP) == SM_IGNORED && at() == 0, "up in a single row does nothing");
  presses(MB_RIGHT, 4);
  CHECK(at() == 4, "right x4");
  CHECK(press(MB_RIGHT) == SM_OK && M.focus == SM_RANDOM, "right at the end of the first row goes to Randomize");
}

static void test_scrolling(void) {
  start(120, ONES, 1, 10); /* 14 rows: 13 full + 3 */
  presses(MB_DOWN, 8);
  CHECK(M.scroll == 0 && M.cursor == 72, "8 downs reach the ninth row (cursor %d scroll %d)", M.cursor, M.scroll);
  CHECK(press(MB_DOWN) == SM_OK && M.scroll == 1 && M.cursor == 72 && at() == 81, "9th down scrolls one row (scroll %d cursor %d)", M.scroll, M.cursor);
  presses(MB_DOWN, 4);
  /* the last rows: 14 rows, the last has 3 species (117, 118, 119); the cursor is in column 0 */
  CHECK(M.scroll == 5 && M.cursor == 72 && at() == 117, "bottom (scroll %d cursor %d at %d)", M.scroll, M.cursor, at());
  CHECK(press(MB_DOWN) == SM_OK && M.scroll == 0 && at() == 0, "down from the bottom wraps to the top (at %d scroll %d)", at(), M.scroll);
  /* up from the top wraps to the bottom, scrolled to the end */
  CHECK(press(MB_UP) == SM_OK && M.scroll == 5 && at() == 117, "up from the top (scroll %d at %d)", M.scroll, at());
  CHECK(press(MB_UP) == SM_OK && at() == 108 && M.scroll == 5, "up (scroll %d at %d)", M.scroll, at());
  presses(MB_UP, 6);
  CHECK(M.scroll == 5 && M.cursor == 9 && at() == 54, "up to the second row on screen (scroll %d cursor %d at %d)", M.scroll, M.cursor, at());
  CHECK(press(MB_UP) == SM_OK && M.scroll == 5 && M.cursor == 0, "up to the top row on screen");
  CHECK(press(MB_UP) == SM_OK && M.scroll == 4 && M.cursor == 0 && at() == 36, "up at the top of the screen scrolls (scroll %d at %d)", M.scroll, at());
}

/* Upstream compares the on-screen cursor with the list's length, so with the list scrolled to its end DOWN from the row above a
 * short last row lands on a cell with no species. The model moves to the first row, as it does unscrolled. */
static void test_scrolled_partial_row(void) {
  start(100, ONES, 1, 10); /* 12 rows; the last has 1 species (99) */
  M.scroll = 3;
  M.cursor = 7 * SM_COLS + 1; /* row 10, column 1: below it would be 100, which is not there */
  CHECK(at() == 91, "set up %d", at());
  sm_result r = press(MB_DOWN);
  CHECK(r == SM_OK && M.scroll == 0 && at() == 1, "down onto a missing cell wraps to the first row (scroll %d at %d)", M.scroll, at());
  M.scroll = 3;
  M.cursor = 7 * SM_COLS;
  r = press(MB_DOWN);
  CHECK(r == SM_OK && at() == 99, "column 0 reaches the last row's one species (%d)", at());
}

/* ---------------------------------------------------------------- the right column */

static void test_leaving_the_grid(void) {
  start(40, ONES, 1, 10);
  /* no team: left from the first row's edge goes to Randomize; from a lower row it wraps */
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_RANDOM, "left from the first row");
  CHECK(sm_species(&M) == 0, "no species on a button");
  CHECK(press(MB_RIGHT) == SM_OK && M.focus == SM_GRID && at() == 0, "right from Randomize to the first species");
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_RANDOM, "left again");
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_GRID && at() == 8, "left from Randomize to the end of the first row (%d)", at());
  /* with a team */
  sm_add(&M, 1);
  sm_add(&M, 2);
  M.focus = SM_GRID;
  M.cursor = 2 * SM_COLS; /* row 2 */
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_PARTY && M.party_cursor == 1, "left from row 2 to team member %d", M.party_cursor);
  CHECK(sm_species(&M) == 2, "panel shows the team member");
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_GRID && at() == 2 * 9 + 8, "left from the team: closest row (%d)", at());
  M.cursor = 7 * SM_COLS + 8;
  CHECK(press(MB_RIGHT) == SM_OK && M.focus == SM_START, "right from row 7 goes to Start (%d)", M.focus);
  CHECK(press(MB_LEFT) == SM_OK && M.focus == SM_GRID && at() == 39 - 0, "left from Start: the last species on screen (%d)", at());
  M.focus = SM_START;
  CHECK(press(MB_RIGHT) == SM_OK && M.focus == SM_GRID && at() == 36, "right from Start: first column of the last row on screen (%d)", at());
}

static void test_team_column(void) {
  start(40, ONES, 1, 10);
  for (int i = 1; i <= 3; i++) sm_add(&M, (uint16_t)i);
  M.focus = SM_RANDOM;
  CHECK(press(MB_DOWN) == SM_OK && M.focus == SM_PARTY && M.party_cursor == 0, "down from Randomize to the first member");
  CHECK(press(MB_DOWN) == SM_OK && M.party_cursor == 1, "down");
  CHECK(press(MB_DOWN) == SM_OK && M.party_cursor == 2, "down");
  CHECK(press(MB_DOWN) == SM_OK && M.focus == SM_START, "down from the last member to Start");
  CHECK(sm_species(&M) == 0, "no species on Start");
  CHECK(press(MB_UP) == SM_OK && M.focus == SM_PARTY && M.party_cursor == 2, "up from Start to the last member");
  CHECK(press(MB_UP) == SM_OK && press(MB_UP) == SM_OK && M.party_cursor == 0, "up");
  CHECK(press(MB_UP) == SM_OK && M.focus == SM_RANDOM, "up from the first member to Randomize");
  CHECK(press(MB_UP) == SM_OK && M.focus == SM_START, "up from Randomize with a team: Start (the filter bar's last filter)");
  CHECK(press(MB_DOWN) == SM_OK && M.focus == SM_RANDOM, "down from Start: Randomize");
  /* no team */
  start(40, ONES, 1, 10);
  M.focus = SM_RANDOM;
  CHECK(press(MB_DOWN) == SM_IGNORED && M.focus == SM_RANDOM, "down from Randomize with no team");
  CHECK(press(MB_UP) == SM_IGNORED && M.focus == SM_RANDOM, "up from Randomize with no team");
}

/* ---------------------------------------------------------------- the team and the budget */

static void test_budget(void) {
  static const double costs[] = {3, 3, 3, 3, 1, 2};
  start(12, costs, 6, 10); /* species 1..12 cost 3,3,3,3,1,2,3,3,3,3,1,2 */
  CHECK(sm_can_add(&M, 1) && sm_add(&M, 1), "add 1 (3)");
  CHECK(sm_add(&M, 2) && sm_add(&M, 3), "add 2, 3 (9)");
  CHECK(sm_value(&M) == 9, "value %g", sm_value(&M));
  CHECK(!sm_can_add(&M, 4) && !sm_add(&M, 4), "4 costs 3: 12 > 10");
  CHECK(!sm_can_add(&M, 1), "no duplicate");
  CHECK(sm_can_add(&M, 5) && sm_add(&M, 5), "5 costs 1: 10 fits exactly");
  CHECK(sm_value(&M) == 10, "full budget");
  CHECK(!sm_add(&M, 11), "nothing more");
  /* what is lit */
  CHECK(!sm_lit(&M, &g_list[3]), "a 3 is greyed with nothing left");
  CHECK(!sm_lit(&M, &g_list[10]), "a 1 is greyed with nothing left");
  sm_remove(&M, 3);
  CHECK(sm_value(&M) == 9 && sm_lit(&M, &g_list[10]) && !sm_lit(&M, &g_list[5]), "after removing: 1 left pays for a 1 only");
  /* six members at most */
  start(12, ONES, 1, 10);
  for (int i = 1; i <= 6; i++) CHECK(sm_add(&M, (uint16_t)i), "add %d", i);
  CHECK(!sm_add(&M, 7), "a seventh is refused");
  CHECK(!sm_can_add(&M, 7), "and not offered");
  /* fractional costs */
  static const double halves[] = {0.5};
  start(12, halves, 1, 10);
  for (int i = 1; i <= 6; i++) sm_add(&M, (uint16_t)i);
  CHECK(sm_value(&M) == 3, "six halves are 3");
}

static void test_options_menu_and_cancel(void) {
  static const double costs[] = {6, 6, 1};
  start(9, costs, 3, 10);
  CHECK(press(MB_ACTION) == SM_MENU, "A on a species that can be added opens the menu");
  CHECK(sm_add(&M, 1), "Add to Party");
  CHECK(press(MB_ACTION) == SM_MENU, "A on a team member: Remove from Party");
  M.cursor = 1; /* species 2 costs 6: 12 > 10 */
  CHECK(press(MB_ACTION) == SM_ERROR, "A on a species that does not fit: nothing to offer");
  /* B removes the last member, then asks to exit */
  M.cursor = 2;
  CHECK(sm_add(&M, 3), "add 3");
  CHECK(M.n_party == 2 && press(MB_CANCEL) == SM_OK && M.n_party == 1 && M.party[0] == 1, "B removes the last");
  CHECK(press(MB_CANCEL) == SM_OK && M.n_party == 0, "B removes the first");
  CHECK(press(MB_CANCEL) == SM_ASK_EXIT, "B with no team asks to exit");
  /* Start */
  CHECK(press(MB_SUBMIT) == SM_ERROR && press(MB_MENU) == SM_ERROR, "Start with no team is refused");
  sm_add(&M, 1);
  CHECK(press(MB_SUBMIT) == SM_ASK_START && press(MB_MENU) == SM_ASK_START, "Start with a team asks");
  M.focus = SM_START;
  CHECK(press(MB_ACTION) == SM_ASK_START, "A on the Start button");
  M.focus = SM_PARTY;
  M.party_cursor = 0;
  CHECK(press(MB_ACTION) == SM_IGNORED, "A on a team member does nothing (upstream handles no ACTION there)");
}

static void test_remove_moves_the_cursor(void) {
  start(40, ONES, 1, 10);
  for (int i = 1; i <= 3; i++) sm_add(&M, (uint16_t)i);
  M.focus = SM_PARTY;
  M.party_cursor = 2;
  sm_remove(&M, 2);
  CHECK(M.focus == SM_PARTY && M.party_cursor == 1, "the cursor follows the team's end");
  sm_remove(&M, 0);
  CHECK(M.focus == SM_PARTY && M.party_cursor == 0 && M.party[0] == 2, "removing an earlier member keeps the index");
  sm_remove(&M, 0);
  CHECK(M.focus == SM_GRID && M.n_party == 0, "the last member removed: back to the list");
  /* Start with the team gone */
  for (int i = 1; i <= 2; i++) sm_add(&M, (uint16_t)i);
  M.focus = SM_START;
  M.scroll = 0;
  M.cursor = 12;
  sm_remove(&M, 1);
  CHECK(M.focus == SM_START, "Start stays while there is a team");
  sm_remove(&M, 0);
  CHECK(M.focus == SM_GRID && at() == 0, "Start with no team goes to the first species on screen (%d)", at());
}

static void test_randomize(void) {
  static const double costs[] = {4, 4, 4, 1, 1, 1};
  start(6, costs, 6, 10);
  M.focus = SM_RANDOM;
  g_rnd_next = 0;
  CHECK(press(MB_ACTION) == SM_OK && M.n_party == 1 && M.party[0] == 1, "random pick 0 = the first valid species (%d)", M.party[0]);
  CHECK(sm_species(&M) == 1 && M.focus == SM_RANDOM, "the panel shows the species, the cursor stays on the button");
  g_rnd_next = 3;
  CHECK(press(MB_ACTION) == SM_OK && M.n_party == 2, "second pick");
  /* valid now: 2 (4) fits? 4+4 = 8 <= 10 yes, 3 yes, 4,5,6 yes; excluding species 1: 5 candidates; pick 3 -> species 5 */
  CHECK(M.party[1] == 5, "pick 3 of the valid species is 5 (got %d)", M.party[1]);
  CHECK(press(MB_DOWN) == SM_OK && sm_species(&M) == M.party[0], "after moving, the panel follows the cursor");
  M.focus = SM_RANDOM;
  /* fill the budget: 4 + 1 = 5; add 4 -> 9, 1 -> 10 */
  sm_add(&M, 2);
  CHECK(sm_value(&M) == 9, "value %g", sm_value(&M));
  g_rnd_next = 0;
  CHECK(press(MB_ACTION) == SM_OK && sm_value(&M) == 10, "the last 1");
  CHECK(press(MB_ACTION) == SM_ERROR, "nothing fits");
  /* a full team */
  static const double zero[] = {0.5};
  start(10, zero, 1, 10);
  M.focus = SM_RANDOM;
  for (int i = 0; i < 6; i++) CHECK(press(MB_ACTION) == SM_OK, "random %d", i);
  CHECK(M.n_party == 6 && press(MB_ACTION) == SM_ERROR, "a seventh is refused");
}

static void test_empty_list(void) {
  start(0, ONES, 1, 10);
  CHECK(M.focus == SM_RANDOM, "no species: the cursor is on a button");
  CHECK(press(MB_LEFT) == SM_IGNORED && press(MB_RIGHT) == SM_IGNORED, "no grid to go to");
  CHECK(press(MB_ACTION) == SM_ERROR, "nothing to pick");
  CHECK(press(MB_CANCEL) == SM_ASK_EXIT, "can still leave");
}

/* every button on every focus, from random positions, must keep the cursor on a real cell and the team legal */
static void test_fuzz(void) {
  static const double costs[] = {1, 2, 3, 4, 5, 0.5, 6};
  unsigned seed = 12345;
  long presses_done = 0;
  for (int run = 0; run < 300; run++) {
    seed = seed * 1103515245u + 12345u;
    const int n = 1 + (int)((seed >> 8) % 200);
    start(n, costs, 7, 10);
    for (int i = 0; i < 400; i++) {
      seed = seed * 1103515245u + 12345u;
      const mui_button b = (mui_button)((seed >> 12) % 9); /* up, down, left, right, submit, action, cancel, menu, stats */
      sm_result r = press(b);
      presses_done++;
      if (r == SM_MENU) {
        uint16_t sp = sm_species(&M);
        if (sm_in_party(&M, sp, NULL)) {
          int idx;
          sm_in_party(&M, sp, &idx);
          sm_remove(&M, idx);
        } else {
          sm_add(&M, sp);
        }
      }
      CHECK(M.n_party >= 0 && M.n_party <= SM_TEAM_MAX, "team size %d", M.n_party);
      CHECK(sm_value(&M) <= 10, "budget %g", sm_value(&M));
      if (M.focus == SM_GRID) {
        CHECK(at() >= 0 && at() < n, "cursor %d scroll %d of %d", M.cursor, M.scroll, n);
        CHECK(sm_cell(&M, M.cursor) != NULL, "cursor on an empty cell");
        CHECK(sm_species(&M) == (uint16_t)(at() + 1), "panel species %d vs %d", sm_species(&M), at() + 1);
      }
      if (M.focus == SM_PARTY) CHECK(M.party_cursor >= 0 && M.party_cursor < M.n_party, "party cursor %d of %d", M.party_cursor, M.n_party);
      CHECK(M.scroll >= 0 && M.scroll * SM_COLS < (n > 0 ? n : 1), "scroll %d", M.scroll);
      for (int a = 0; a < M.n_party; a++)
        for (int c = a + 1; c < M.n_party; c++) CHECK(M.party[a] != M.party[c], "duplicate in the team");
    }
  }
  printf("fuzz: %ld button presses over 300 lists\n", presses_done);
}

int main(void) {
  test_icon_positions();
  test_closest_party_member();
  test_value_text();
  test_grid_moves();
  test_single_row();
  test_scrolling();
  test_scrolled_partial_row();
  test_leaving_the_grid();
  test_team_column();
  test_budget();
  test_options_menu_and_cancel();
  test_remove_moves_the_cursor();
  test_randomize();
  test_empty_list();
  test_fuzz();
  printf("startertest: %d checks, %d failed\n", g_checks, g_fail);
  return g_fail ? 1 : 0;
}
