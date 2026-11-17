/*
 * events_view: what the title screen says about the timed events (custom-core's cc/tevents.h): which are on now, how long they
 * stay, and which open within the next day. Pure: the console's local time is an argument (events_host.h's eh_now() gives it), so
 * the host test pins it. The drawing is title_screen.c's.
 */
#ifndef GM_EVENTS_VIEW_H
#define GM_EVENTS_VIEW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cc/tevents.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EV_ROWS 16            /* custom-core's CC_TEVENTS_MAX */
#define EV_NAME 32
#define EV_BLURB 56
#define EV_COMING_SECONDS 86400   /* an event that opens later than a day from now is not listed */
#define EV_ROW_H 23           /* the panel's pixels per row, and per group label ("ON NOW", "COMING UP") */
#define EV_LABEL_H 11
#define EV_PAGES_MAX EV_ROWS

typedef struct {
  char name[EV_NAME];
  char blurb[EV_BLURB];   /* what it changes, one short line */
  int64_t secs;           /* on: seconds until it closes; coming: seconds until it opens (always > 0) */
  int day_offset;         /* coming: 0 = it opens today, 1 = tomorrow */
  int start_hour, start_minute;   /* coming: when it opens, the console's local time */
} ev_row;

typedef struct {
  bool ok;                /* false: the clock could not be read, nothing below is valid */
  int n_on, n_next;
  ev_row on[EV_ROWS];     /* the ones open now, the one that closes first at the top */
  ev_row next[EV_ROWS];   /* the ones that open within EV_COMING_SECONDS, the first to open at the top */
} ev_view;

/* The view at local_s (eh_local_seconds' unit). A negative or out-of-range time gives ok = false and no rows. */
void ev_build(int64_t local_s, ev_view *out);

/* The panel lists the open events first and then the ones about to open, as pages: a page holds as many rows as fit in `avail` pixels
 * (EV_ROW_H a row, EV_LABEL_H for a label above the first row of each group on the page), and the title turns the pages by the clock.
 * Row i of the flat list is on[i] for i < n_on, else next[i - n_on]. ev_layout fills page_start[0..n-1] with the flat index of each
 * page's first row and returns n (0 when the view is not ok or has no rows). Every row is on exactly one page, in order, and a page is
 * never empty, even when one row alone does not fit. */
int ev_layout(const ev_view *v, int avail, int *page_start, int max_pages);
/* The row at flat index i (and whether it is an open one), NULL when out of range. */
const ev_row *ev_item(const ev_view *v, int i, bool *on);

/* The line saying what an event changes: the numbers are written from its effects (so a table change shows), the item and species
 * ones are worded here by event id. An event with nothing to say gets EV_BLURB_NONE (the host test fails on it, so a new event cannot
 * ship without its line). */
#define EV_BLURB_NONE "A special event"
void ev_blurb(const cc_tevent *e, char *out, size_t n);

/* "30 min", "3 h 30 min", "5 h": the time rounded up to the minute. */
void ev_left(int64_t secs, char *out, size_t n);

/* "today 20:00" or "tomorrow 00:00" for a coming row. */
void ev_when(const ev_row *r, char *out, size_t n);

/* The two lines of the top-screen banner: the event that closes first (or, when none is on, the next to open). The title is short (it is
 * drawn in the large font), what it changes and how long it stays go on the small line. */
void ev_headline(const ev_view *v, char *title, size_t tn, char *sub, size_t sn);

#ifdef __cplusplus
}
#endif
#endif
