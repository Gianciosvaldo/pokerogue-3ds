/* The starter select screen's input rules (see starter_model.h). Line references are to
 * src/ui/handlers/starter-select-ui-handler.ts at the pinned upstream commit (8555c08). */
#include "starter_model.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* teamWindowY / teamWindowHeight (:103-105) */
#define TEAM_WINDOW_Y 38
#define TEAM_WINDOW_HEIGHT 107

/* calcStarterIconY (:127): Math.round(firstStarterY + starterSpacing * index), in the same double arithmetic */
int sm_party_icon_y(int i) {
  const double spacing = (double)TEAM_WINDOW_HEIGHT / 7;
  const double first = TEAM_WINDOW_Y + spacing / 2;
  return (int)floor(first + spacing * i + 0.5);
}

/* findClosestStarterIndex (:139): the team member whose icon is level with `y` (a grid row's y) */
static int closest_party(int y, int team) {
  double smallest = TEAM_WINDOW_HEIGHT;
  int best = 0;
  for (int i = 0; i < team; i++) {
    const double d = fabs((double)y - (double)(sm_party_icon_y(i) - 13));
    if (d < smallest) {
      best = i;
      smallest = d;
    }
  }
  return best;
}

/* calcStarterContainerPosition (:113): the y of grid row r is 13 + r * 17 (SM_CELL_H here) */
static int row_y(int row) { return 13 + row * SM_CELL_H; }

/* processPartyIconInput's closestRowIndex (:1386): upstream takes member i + 1, the row level with the member's icon when rows are
 * 17 px apart. The same row for any row height: the one whose y is nearest the icon's (which gives i + 1 at 17 px). */
static int party_row(int i) {
  const int r = (int)floor((double)(sm_party_icon_y(i) - 13 - 13) / SM_CELL_H + 0.5);
  return r < SM_ROWS - 1 ? r : SM_ROWS - 1;
}

/* ---------------------------------------------------------------- the list */

static int imin(int a, int b) { return a < b ? a : b; }

static int abs_index(const sm_model* m) { return m->scroll * SM_COLS + m->cursor; }

const sm_entry* sm_cell(const sm_model* m, int i) {
  const int k = m->scroll * SM_COLS + i;
  return i >= 0 && i < SM_PAGE && k < m->n ? &m->entries[k] : NULL;
}

const sm_entry* sm_find(const sm_model* m, uint16_t species) {
  int lo = 0, hi = m->n - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (m->entries[mid].species == species) return &m->entries[mid];
    if (m->entries[mid].species < species)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return NULL;
}

/* setCursor (:2621) for the grid: clamped to the containers (and, as upstream's containers hide, to the species there are).
 * True when it moved. */
static bool set_cursor(sm_model* m, int c) {
  if (c > SM_PAGE - 1) c = SM_PAGE - 1;
  if (c < 0) c = 0;
  const int last = m->n - m->scroll * SM_COLS - 1;
  if (c > last) c = last > 0 ? last : 0;
  const bool changed = c != m->cursor;
  m->cursor = c;
  return changed;
}

/* the last species on screen (onScreenLastIndex, :1001) */
static int on_screen_last(const sm_model* m) {
  const int v = m->n - m->scroll * SM_COLS - 1;
  return v < SM_PAGE - 1 ? v : SM_PAGE - 1;
}

/* ---------------------------------------------------------------- the team */

bool sm_in_party(const sm_model* m, uint16_t species, int* index) {
  for (int i = 0; i < m->n_party; i++)
    if (m->party[i] == species) {
      if (index) *index = i;
      return true;
    }
  return false;
}

double sm_value(const sm_model* m) {
  double v = 0;
  for (int i = 0; i < m->n_party; i++) {
    const sm_entry* e = sm_find(m, m->party[i]);
    if (e) v += e->cost;
  }
  return v;
}

bool sm_can_add(const sm_model* m, uint16_t species) {
  const sm_entry* e = sm_find(m, species);
  if (!e || sm_in_party(m, species, NULL) || m->n_party >= SM_TEAM_MAX) return false;
  return sm_value(m) + e->cost <= (double)m->budget; /* :1595 */
}

bool sm_lit(const sm_model* m, const sm_entry* e) { return (double)m->budget - sm_value(m) >= e->cost; /* :3065-3106 */ }

bool sm_add(sm_model* m, uint16_t species) {
  if (!sm_can_add(m, species)) return false;
  m->party[m->n_party++] = species;
  return true;
}

/* popPartyStarter (:2952): the cursor on a removed member, or on Start with no team left, goes back to the list */
void sm_remove(sm_model* m, int index) {
  if (index < 0 || index >= m->n_party) return;
  memmove(&m->party[index], &m->party[index + 1], (size_t)(m->n_party - index - 1) * sizeof m->party[0]);
  m->n_party--;
  if (m->focus == SM_PARTY) {
    if (m->party_cursor == m->n_party) {
      if (m->n_party > 0)
        m->party_cursor--;
      else
        m->focus = m->n > 0 ? SM_GRID : SM_RANDOM; /* "back to filters": the list, as the filter bar's DOWN leads to it */
    }
  } else if (m->focus == SM_START && m->n_party == 0) {
    if (m->n > 0) {
      m->focus = SM_GRID;
      set_cursor(m, m->scroll * SM_COLS);
    } else {
      m->focus = SM_RANDOM;
    }
  }
}

/* ---------------------------------------------------------------- input */

/* :1430 processBoxInput */
static sm_result box_input(sm_model* m, mui_button b) {
  const int n = m->n;
  const int rows = (n + SM_COLS - 1) / SM_COLS;
  const int first = m->scroll * SM_COLS;
  const int cur_row = (first + m->cursor) / SM_COLS;
  const int on_row = m->cursor / SM_COLS;
  bool ok = false;

  switch (b) {
    case MB_UP:
      if (cur_row > 0) {
        if (m->scroll > 0 && cur_row - m->scroll == 0) {
          m->scroll--;
          set_cursor(m, m->cursor);
          ok = true;
        } else {
          ok = set_cursor(m, m->cursor - SM_COLS);
        }
        break;
      }
      /* the first row: upstream goes to the filter bar. Without one it wraps to the last row, the way the filter bar's UP
       * lands (:943-957), in the same column or the one above when the last row is short */
      if (rows > 1) {
        const int col = (first + m->cursor) % SM_COLS;
        int target = (rows - 1) * SM_COLS + col;
        if (target >= n) target -= SM_COLS;
        m->scroll = rows > SM_ROWS ? rows - SM_ROWS : 0;
        ok = true;
        set_cursor(m, target - m->scroll * SM_COLS);
      }
      break;
    case MB_DOWN:
      /* :1460. Upstream compares the on-screen cursor with the list's length; the cell below is first + cursor + SM_COLS */
      if (cur_row < rows - 1 && first + m->cursor + SM_COLS < n) {
        if (cur_row - m->scroll == SM_ROWS - 1) {
          m->scroll++;
          set_cursor(m, m->cursor);
          ok = true;
        } else {
          ok = set_cursor(m, m->cursor + SM_COLS);
        }
        break;
      }
      if (rows > 1) { /* the last row wraps to the first */
        m->scroll = 0;
        ok = set_cursor(m, m->cursor % SM_COLS);
      }
      break; /* a single row: upstream goes to the filter bar */
    case MB_LEFT:
    case MB_RIGHT: {
      const bool left = b == MB_LEFT;
      if (left ? m->cursor % SM_COLS != 0 : m->cursor % SM_COLS < (cur_row < rows - 1 ? SM_COLS - 1 : (n - 1) % SM_COLS)) {
        ok = set_cursor(m, m->cursor + (left ? -1 : 1));
        break;
      }
      /* an edge of the list */
      if (on_row == 0) {
        m->focus = SM_RANDOM; /* from the first row to the random selection */
      } else if (m->n_party == 0) {
        /* no team: wrap around the row */
        const int step = left ? imin(SM_COLS - 1, on_screen_last(m) - m->cursor) : -imin(SM_COLS - 1, m->cursor % SM_COLS);
        ok = set_cursor(m, m->cursor + step);
        break;
      } else if (on_row < SM_PARTY_ROWS) {
        m->focus = SM_PARTY; /* the team member level with the row */
        m->party_cursor = closest_party(row_y(on_row), m->n_party);
      } else {
        m->focus = SM_START; /* the rows below the team (upstream's bottom two) go to the Start button */
      }
      ok = true;
      break;
    }
    default:
      break;
  }
  return ok ? SM_OK : SM_IGNORED;
}

/* :1369 processPartyIconInput */
static sm_result party_input(sm_model* m, mui_button b) {
  const int n = m->n;
  const int last = n - 1 < SM_PAGE - 1 ? n - 1 : SM_PAGE - 1; /* note: upstream does not subtract the scroll here */
  switch (b) {
    case MB_UP:
      if (m->party_cursor == 0)
        m->focus = SM_RANDOM;
      else
        m->party_cursor--;
      return SM_OK;
    case MB_DOWN:
      if (m->party_cursor <= m->n_party - 2)
        m->party_cursor++;
      else
        m->focus = SM_START;
      return SM_OK;
    case MB_LEFT:
    case MB_RIGHT: {
      if (n <= 0) return SM_IGNORED;
      const int closest_row = party_row(m->party_cursor);
      m->focus = SM_GRID;
      if (b == MB_LEFT)
        set_cursor(m, closest_row * SM_COLS + SM_COLS - 1 < last ? closest_row * SM_COLS + SM_COLS - 1 : last);
      else
        set_cursor(m, closest_row * SM_COLS < last - last % SM_COLS ? closest_row * SM_COLS : last - last % SM_COLS);
      return SM_OK;
    }
    default:
      return SM_IGNORED;
  }
}

/* :994 processStartCursorInput */
static sm_result start_input(sm_model* m, mui_button b) {
  const int last = on_screen_last(m);
  switch (b) {
    case MB_UP:
      if (m->n_party > 0) {
        m->focus = SM_PARTY;
        m->party_cursor = m->n_party - 1;
      } else {
        m->focus = SM_RANDOM;
      }
      return SM_OK;
    case MB_DOWN: /* to the filter bar, whose last filter's DOWN is the Randomize button */
      m->focus = SM_RANDOM;
      return SM_OK;
    case MB_LEFT:
      if (m->n <= 0) return SM_IGNORED;
      m->focus = SM_GRID;
      set_cursor(m, last);
      return SM_OK;
    case MB_RIGHT: {
      if (m->n <= 0) return SM_IGNORED;
      const int rows_on_screen = (last + SM_COLS - 1) / SM_COLS; /* ceil(onScreenLastIndex / 9) with SM_COLS for 9, as upstream has it */
      m->focus = SM_GRID;
      set_cursor(m, (rows_on_screen - 1) * SM_COLS);
      return SM_OK;
    }
    default:
      return SM_IGNORED;
  }
}

/* :1055 processRandomCursorInput. The ACTION case is in sm_input (it needs to set the panel). */
static sm_result random_input(sm_model* m, mui_button b) {
  switch (b) {
    case MB_UP: /* to the filter bar, whose last filter's UP is the Start button (with a team) or this button */
      if (m->n_party > 0) {
        m->focus = SM_START;
        return SM_OK;
      }
      return SM_IGNORED;
    case MB_DOWN:
      if (m->n_party > 0) {
        m->focus = SM_PARTY;
        m->party_cursor = 0;
        return SM_OK;
      }
      return SM_IGNORED; /* no team: the filter bar, whose last filter's DOWN is this button again */
    case MB_LEFT:
      if (m->n <= 0) return SM_IGNORED;
      m->focus = SM_GRID;
      set_cursor(m, SM_COLS - 1 < m->n - 1 ? SM_COLS - 1 : m->n - 1); /* the end of the first row (upstream's 8) */
      return SM_OK;
    case MB_RIGHT:
      if (m->n <= 0) return SM_IGNORED;
      m->focus = SM_GRID;
      set_cursor(m, 0);
      return SM_OK;
    default:
      return SM_IGNORED;
  }
}

/* Randomize (:1062): a species not in the team that the budget pays for */
static sm_result randomize(sm_model* m, uint16_t* picked) {
  if (m->n_party >= SM_TEAM_MAX) return SM_ERROR;
  int valid = 0;
  for (int i = 0; i < m->n; i++)
    if (sm_can_add(m, m->entries[i].species)) valid++;
  if (valid == 0) return SM_ERROR;
  int pick = m->rnd ? m->rnd(m->rnd_ctx, valid) : 0;
  if (pick < 0 || pick >= valid) pick = 0;
  for (int i = 0; i < m->n; i++) {
    if (!sm_can_add(m, m->entries[i].species)) continue;
    if (pick-- == 0) {
      *picked = m->entries[i].species;
      sm_add(m, *picked);
      return SM_OK;
    }
  }
  return SM_ERROR;
}

/* what the panel shows for the cursor */
static uint16_t species_at_cursor(const sm_model* m) {
  if (m->focus == SM_GRID) {
    const int k = abs_index(m);
    return k >= 0 && k < m->n ? m->entries[k].species : 0;
  }
  if (m->focus == SM_PARTY) return m->party_cursor < m->n_party ? m->party[m->party_cursor] : 0;
  return 0;
}

uint16_t sm_species(const sm_model* m) { return m->shown ? m->shown : species_at_cursor(m); }

void sm_init(sm_model* m, const sm_entry* entries, int n, int budget, int (*rnd)(void*, int), void* rnd_ctx) {
  memset(m, 0, sizeof *m);
  m->entries = entries;
  m->n = n;
  m->budget = budget;
  m->rnd = rnd;
  m->rnd_ctx = rnd_ctx;
  m->focus = n > 0 ? SM_GRID : SM_RANDOM;
}

/* :2088 processInput */
sm_result sm_input(sm_model* m, mui_button b) {
  sm_result r = SM_IGNORED;
  uint16_t random_pick = 0;

  if (b == MB_SUBMIT || b == MB_MENU) { /* tryStart(true) */
    r = m->n_party == 0 ? SM_ERROR : SM_ASK_START;
  } else if (b == MB_CANCEL) {
    if (m->n_party > 0) {
      sm_remove(m, m->n_party - 1);
      r = SM_OK;
    } else {
      r = SM_ASK_EXIT;
    }
  } else if (m->focus == SM_PARTY) {
    r = party_input(m, b);
  } else if (m->focus == SM_START) {
    r = b == MB_ACTION ? (m->n_party == 0 ? SM_ERROR : SM_ASK_START) : start_input(m, b);
  } else if (m->focus == SM_RANDOM) {
    r = b == MB_ACTION ? randomize(m, &random_pick) : random_input(m, b);
  } else if (b == MB_ACTION) {
    /* the options menu opens on a caught species. Upstream's has more options than the two this screen has: with neither
     * Add to Party nor Remove from Party to offer, there is nothing to open */
    const uint16_t sp = species_at_cursor(m);
    r = sp && (sm_can_add(m, sp) || sm_in_party(m, sp, NULL)) ? SM_MENU : SM_ERROR;
  } else {
    r = box_input(m, b);
  }
  /* the panel follows the cursor, except after Randomize: the species just added shows while the cursor stays on the button */
  m->shown = random_pick;
  return r;
}

/* ---------------------------------------------------------------- text */

/* JavaScript's text for a number that is a sum of halves and quarters, and tryUpdateValue's "0." -> "." */
void sm_cost_text(char* out, int n, double v) {
  char buf[32];
  snprintf(buf, sizeof buf, "%.6f", v);
  char* dot = strchr(buf, '.');
  if (dot) {
    char* end = buf + strlen(buf) - 1;
    while (end > dot && *end == '0') *end-- = 0;
    if (end == dot) *end = 0;
  }
  snprintf(out, (size_t)n, "%s", buf[0] == '0' && buf[1] == '.' ? buf + 1 : buf);
}

void sm_value_text(char* out, int n, double value, int budget) {
  char v[32];
  sm_cost_text(v, sizeof v, value);
  snprintf(out, (size_t)n, "%s/%d", v, budget);
}

int sm_cost_style(const sm_entry* e) {
  const double d = e->base_cost - e->cost;
  if (d == 0) return 0;
  if (d == 1 || d == 0.5) return 1;
  return 2;
}
