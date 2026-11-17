/* The title screen's events lines (see events_view.h). */
#include "events_view.h"

#include <stdio.h>
#include <string.h>

/* What the items and species events say, by id: their effects are lists, so the line is worded by hand. */
static const struct {
  const char *id, *text;
} WORDED[] = {
    {"charm_hour", "Charms after waves 8 and 25"},
    {"lucky_species_hour", "Eevee line: luck +1 each"},
    {"candy_jar_day", "Candy Jar after wave 25"},
    {"delibirdy_day", "Delibird-y is common, +1 charm"},
    {"fusion_day", "Fusions feed candy faster"},
    {"waiting_room", "Mega + Dynamax items at wave 115"},
    {"snowy_town", "The Town is always snowing"},
};

static void put(char *out, size_t n, const char *s) {
  if (!n) return;
  snprintf(out, n, "%s", s);
}

void ev_blurb(const cc_tevent *e, char *out, size_t n) {
  if (!n) return;
  out[0] = 0;
  if (!e) {
    put(out, n, EV_BLURB_NONE);
    return;
  }
  for (size_t i = 0; i < sizeof WORDED / sizeof WORDED[0]; i++)
    if (e->id && !strcmp(e->id, WORDED[i].id)) {
      put(out, n, WORDED[i].text);
      return;
    }
  /* the numeric effects, in the order the header lists them, joined with "; " */
  const cc_tevent_effects *f = &e->fx;
  char t[EV_BLURB];
  t[0] = 0;
#define ADD(...)                                                      \
  do {                                                                \
    char one[48];                                                     \
    snprintf(one, sizeof one, __VA_ARGS__);                           \
    size_t l = strlen(t);                                             \
    snprintf(t + l, sizeof t - l, "%s%s", l ? "; " : "", one);        \
  } while (0)
  if (f->shiny_encounter_mult) ADD("Wild shiny odds x%u", (unsigned)f->shiny_encounter_mult);
  if (f->shiny_catch_mult) ADD("Shiny catch rate x%u", (unsigned)f->shiny_catch_mult);
  if (f->candy_mult) ADD("Classic candy rate x%u", (unsigned)f->candy_mult);
  if (f->luck_boost) ADD("Party luck +%u", (unsigned)f->luck_boost);
  if (f->trainer_shiny_chance) ADD("Trainer shiny chance 1 in %u", (unsigned)((65536u + f->trainer_shiny_chance / 2) / f->trainer_shiny_chance));
  if (f->upgrade_vouchers) ADD("Bosses give the upgraded voucher");
#undef ADD
  put(out, n, t[0] ? t : EV_BLURB_NONE);
}

void ev_left(int64_t secs, char *out, size_t n) {
  if (secs < 0) secs = 0;
  const int64_t m = (secs + 59) / 60;
  if (m >= 60) {
    if (m % 60) snprintf(out, n, "%d h %d min", (int)(m / 60), (int)(m % 60));
    else snprintf(out, n, "%d h", (int)(m / 60));
  } else {
    snprintf(out, n, "%d min", (int)m);
  }
}

void ev_when(const ev_row *r, char *out, size_t n) {
  snprintf(out, n, "%s%s%02d:%02d", r->day_offset == 0 ? "today" : r->day_offset == 1 ? "tomorrow" : "", r->day_offset <= 1 ? " " : "",
           r->start_hour, r->start_minute);
}

static void fill(ev_row *r, const cc_tevent *e, int64_t secs) {
  memset(r, 0, sizeof *r);
  snprintf(r->name, sizeof r->name, "%s", e->name ? e->name : "");
  ev_blurb(e, r->blurb, sizeof r->blurb);
  r->secs = secs;
}

/* insertion sort by secs, keeping the table's order among equals */
static void add_sorted(ev_row *rows, int *n, const ev_row *r) {
  int i = *n;
  while (i > 0 && rows[i - 1].secs > r->secs) {
    rows[i] = rows[i - 1];
    i--;
  }
  rows[i] = *r;
  (*n)++;
}

void ev_build(int64_t local_s, ev_view *out) {
  memset(out, 0, sizeof *out);
  if (local_s < 0 || local_s > CC_TEVENT_TIME_MAX) return;
  out->ok = true;
  for (unsigned i = 0; i < cc_tevent_count && i < EV_ROWS; i++) {
    int active = 0;
    int64_t start = 0, end = 0;
    if (cc_tevent_window(&cc_tevents[i].when, local_s, &active, &start, &end) != CC_OK) continue;
    ev_row r;
    if (active) {
      fill(&r, &cc_tevents[i], end - local_s);
      add_sorted(out->on, &out->n_on, &r);
    } else if (start - local_s <= EV_COMING_SECONDS) {
      fill(&r, &cc_tevents[i], start - local_s);
      r.day_offset = (int)(start / 86400 - local_s / 86400);
      r.start_hour = (int)(start % 86400 / 3600);
      r.start_minute = (int)(start % 3600 / 60);
      add_sorted(out->next, &out->n_next, &r);
    }
  }
}

void ev_headline(const ev_view *v, char *title, size_t tn, char *sub, size_t sn) {
  char left[24], when[32];
  if (!v->ok) {
    snprintf(title, tn, "No event today");
    snprintf(sub, sn, "The console clock cannot be read.");
  } else if (v->n_on > 0) {
    const ev_row *r = &v->on[0];
    ev_left(r->secs, left, sizeof left);
    snprintf(title, tn, "%s is on", r->name);
    snprintf(sub, sn, "%s. %s left.", r->blurb, left);
  } else if (v->n_next > 0) {
    const ev_row *r = &v->next[0];
    ev_left(r->secs, left, sizeof left);
    ev_when(r, when, sizeof when);
    snprintf(title, tn, "Next: %s %s", r->name, when);
    snprintf(sub, sn, "In %s. %s.", left, r->blurb);
  } else {
    snprintf(title, tn, "No event is on");
    snprintf(sub, sn, "More open on other days.");
  }
}

const ev_row *ev_item(const ev_view *v, int i, bool *on) {
  if (i < 0 || i >= v->n_on + v->n_next) return NULL;
  if (on) *on = i < v->n_on;
  return i < v->n_on ? &v->on[i] : &v->next[i - v->n_on];
}

int ev_layout(const ev_view *v, int avail, int *page_start, int max_pages) {
  if (!v->ok) return 0;
  const int total = v->n_on + v->n_next;
  int pages = 0, used = 0, group = -1;
  for (int i = 0; i < total; i++) {
    const int g = i < v->n_on ? 0 : 1;
    int extra = g != group ? EV_LABEL_H : 0;
    if (pages == 0 || used + extra + EV_ROW_H > avail) {   /* the first row, or one that does not fit: a page of its own start, with its label */
      if (pages >= max_pages) return pages;
      page_start[pages++] = i;
      used = 0;
      extra = EV_LABEL_H;
    }
    used += extra + EV_ROW_H;
    group = g;
  }
  return pages;
}
