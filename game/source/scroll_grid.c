/* See scroll_grid.h. A line-by-line port of ScrollableGridHelper; the names follow upstream's. */
#include "scroll_grid.h"

#include <stddef.h>

static int ceil_div(int a, int b) { return a <= 0 ? 0 : (a + b - 1) / b; }
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

void sg_init(scroll_grid* g, int rows, int cols, const scroll_grid_hooks* hooks, void* ud) {
  g->rows = rows;
  g->cols = cols;
  g->total = rows * cols;
  g->cursor = 0;
  g->scroll = 0;
  g->hooks = *hooks;
  g->ud = ud;
}

static bool set_cursor(scroll_grid* g, int cursor) {
  g->cursor = cursor;
  return g->hooks.set_cursor ? g->hooks.set_cursor(g->ud, cursor) : false;
}

/* has_cursor false: upstream's `cursor === undefined` */
static bool set_scroll_cursor(scroll_grid* g, int scroll, int cursor, bool has_cursor) {
  const bool scroll_changed = scroll != g->scroll;
  if (scroll_changed) {
    g->scroll = scroll;
    if (g->hooks.update_grid) g->hooks.update_grid(g->ud);
  }
  bool cursor_changed = false;
  const int new_element = g->cursor + g->scroll * g->cols;
  if (has_cursor) {
    cursor_changed = set_cursor(g, cursor);
  } else if (new_element >= g->total) {
    /* the cursor must not go past the end of the list */
    cursor_changed = set_cursor(g, g->total - g->scroll * g->cols - 1);
  } else if (scroll_changed && g->hooks.update_detail) {
    g->hooks.update_detail(g->ud, new_element);
  }
  return scroll_changed || cursor_changed;
}

void sg_set_total(scroll_grid* g, int total) {
  g->total = total;
  set_scroll_cursor(g, 0, 0, false);
}

int sg_item_offset(const scroll_grid* g) { return g->scroll * g->cols; }

bool sg_input(scroll_grid* g, sg_dir d) {
  if (g->total <= 0) return false; /* nothing to move over (upstream never gets here: it opens only with eggs) */
  bool success = false;
  const int total_rows = ceil_div(g->total, g->cols);
  const int on_screen_rows = imin(g->rows, total_rows);
  const int max_scroll = imax(0, total_rows - on_screen_rows);
  const int row = g->cursor / g->cols;
  const int col = g->cursor % g->cols;
  const int item_offset = g->scroll * g->cols;
  const int last_visible = imin(g->total - 1, g->total - max_scroll * g->cols - 1);
  switch (d) {
    case SG_UP:
      if (row > 0) {
        success = set_cursor(g, g->cursor - g->cols);
      } else if (g->scroll > 0) {
        success = set_scroll_cursor(g, g->scroll - 1, 0, false);
      } else {
        /* wrap around to the last row */
        int new_cursor = g->cursor + (on_screen_rows - 1) * g->cols;
        if (new_cursor > last_visible) new_cursor -= g->cols;
        success = set_scroll_cursor(g, max_scroll, new_cursor, true);
      }
      break;
    case SG_DOWN:
      if (row < on_screen_rows - 1) {
        success = set_cursor(g, imin(g->cursor + g->cols, g->total - item_offset - 1));
      } else if (g->scroll < max_scroll) {
        success = set_scroll_cursor(g, g->scroll + 1, 0, false);
      } else {
        /* wrap around to the top row */
        success = set_scroll_cursor(g, 0, g->cursor % g->cols, true);
      }
      break;
    case SG_LEFT:
      if (col > 0) {
        success = set_cursor(g, g->cursor - 1);
      } else if (g->scroll == max_scroll && row == on_screen_rows - 1) {
        success = set_cursor(g, last_visible);
      } else {
        success = set_cursor(g, g->cursor + g->cols - 1);
      }
      break;
    case SG_RIGHT:
      if (col < g->cols - 1 && g->cursor + item_offset < g->total - 1) {
        success = set_cursor(g, g->cursor + 1);
      } else {
        success = set_cursor(g, g->cursor - col);
      }
      break;
  }
  return success;
}

void sg_reset(scroll_grid* g) {
  set_scroll_cursor(g, 0, 0, false);
  set_cursor(g, 0);
}
