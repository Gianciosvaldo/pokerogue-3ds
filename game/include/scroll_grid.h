/*
 * scroll_grid: upstream's ScrollableGridHelper (src/ui/utils/scrollable-grid-helper.ts) as a plain model, the cursor and scrolling of a grid of
 * `rows` x `cols` cells over `total` elements that scrolls one row at a time (the Egg List; the Pokedex list has its own navigation, upstream does not use this helper for it). No drawing: the owner
 * gives three hooks, exactly the calls the upstream helper makes on its handler.
 *
 *   set_cursor(ud, c)       the handler's setCursor: true when its own cursor changed (the helper's return value is built from this)
 *   update_grid(ud)         the scroll position changed: redraw the whole grid (may be NULL)
 *   update_detail(ud, i)    the scroll changed but the cursor did not: the element under it (index into the whole list) is another one (may be NULL)
 *
 * Fresh state, like the upstream constructor: cursor 0, scroll 0, total = rows * cols. A screen calls sg_set_total when it is shown.
 * sg_input returns what processInput does: true when the cursor or the scroll position changed.
 */
#ifndef GM_SCROLL_GRID_H
#define GM_SCROLL_GRID_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SG_UP, SG_DOWN, SG_LEFT, SG_RIGHT } sg_dir; /* the order of upstream's Button enum */

typedef struct {
  bool (*set_cursor)(void* ud, int cursor);
  void (*update_grid)(void* ud);
  void (*update_detail)(void* ud, int index);
} scroll_grid_hooks;

typedef struct {
  int rows, cols, total;
  int cursor, scroll; /* the cursor is inside the visible rows; the first visible element is scroll * cols */
  scroll_grid_hooks hooks;
  void* ud;
} scroll_grid;

void sg_init(scroll_grid* g, int rows, int cols, const scroll_grid_hooks* hooks, void* ud);
void sg_set_total(scroll_grid* g, int total); /* setTotalElements: also scrolls to the top */
int sg_item_offset(const scroll_grid* g);     /* getItemOffset: the number of elements scrolled off the top */
bool sg_input(scroll_grid* g, sg_dir d);
void sg_reset(scroll_grid* g);                /* reset: scroll 0, cursor 0 */

#ifdef __cplusplus
}
#endif
#endif
