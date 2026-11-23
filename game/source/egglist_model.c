/* What the Egg List shows for an egg (see egglist_model.h). */
#include "egglist_model.h"

#include <stdio.h>
#include <string.h>

#include "gc/data.h"
#include "gc/gacha.h"

#define SP_PHIONE 489
#define SP_MANAPHY 490
#define EGG_TIER_COMMON 0
#define SOURCE_GACHA_MOVE 0
#define SOURCE_GACHA_LEGENDARY 1
#define SOURCE_GACHA_SHINY 2
#define SOURCE_SAME_SPECIES 3
#define SOURCE_EVENT 4
#define JS_MAX_TIME_MS UINT64_C(8640000000000000) /* the largest time a JavaScript Date holds */

bool egm_is_manaphy(const gc_egg* e) {
  return e->species == SP_PHIONE || e->species == SP_MANAPHY || (e->tier == EGG_TIER_COMMON && e->id % 204u == 0 && e->species == 0);
}

/* days since 1970-01-01 -> year, month 1..12, day 1..31 (Howard Hinnant's civil_from_days) */
static void civil(int64_t z, int64_t* y, int* m, int* d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t yy = yoe + era * 400;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  *d = (int)(doy - (153 * mp + 2) / 5 + 1);
  *m = (int)(mp < 10 ? mp + 3 : mp - 9);
  *y = yy + (*m <= 2);
}

void egm_date(uint64_t timestamp_ms, int tz_offset_s, char* out, int n) {
  static const char* const WEEKDAY[7] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"}; /* 1970-01-01 was a Thursday */
  if (timestamp_ms > JS_MAX_TIME_MS) {
    snprintf(out, (size_t)n, "Invalid Date");
    return;
  }
  /* floor to the second in local time; the time is not negative but the local one can be (before 1970-01-01 plus the offset) */
  const int64_t local_s = (int64_t)(timestamp_ms / 1000u) + tz_offset_s;
  int64_t days = local_s / 86400;
  if (local_s % 86400 < 0) days--;
  int64_t y;
  int m, d;
  civil(days, &y, &m, &d);
  int64_t wd = days % 7;
  if (wd < 0) wd += 7;
  snprintf(out, (size_t)n, "%s, %02d/%d/%lld", WEEKDAY[wd], m, d, (long long)y);
}

/* the text with every "{{species}}" replaced by the name (i18next with escaping off) */
static void interpolate(char* out, int n, const char* tmpl, const char* species) {
  int o = 0;
  const char* p = tmpl;
  while (*p && o < n - 1) {
    if (!strncmp(p, "{{species}}", 11)) {
      for (const char* s = species; *s && o < n - 1; s++) out[o++] = *s;
      p += 11;
    } else {
      out[o++] = *p++;
    }
  }
  out[o] = 0;
}

void egm_key(const gc_egg* e, char* out, int n) {
  if (egm_is_manaphy(e))
    snprintf(out, (size_t)n, "manaphy");
  else
    snprintf(out, (size_t)n, "%u", (unsigned)e->tier);
}

void egm_describe(const gc_egg* e, const egm_texts* t, egm_species_name name, void* ud, int tz_offset_s, egm_details* out) {
  const bool manaphy = egm_is_manaphy(e);
  egm_key(e, out->key, sizeof out->key);
  /* Egg.getEggDescriptor */
  const char* tier = t->tier[0];
  if (manaphy) tier = t->tier[4];
  else if (e->tier == 1) tier = t->tier[1];
  else if (e->tier == 2) tier = t->tier[2];
  else if (e->tier == 3) tier = t->tier[3];
  snprintf(out->name, sizeof out->name, "%s (%s)", t->egg, tier);
  egm_date(e->timestamp_ms, tz_offset_s, out->date, sizeof out->date);
  /* Egg.getEggHatchWavesMessage */
  const int w = e->hatch_waves;
  snprintf(out->waves, sizeof out->waves, "%s", t->waves[w <= 5 ? 0 : w <= 15 ? 1 : w <= 50 ? 2 : 3]);
  /* Egg.getEggTypeDescriptor */
  out->type[0] = 0;
  switch (e->source_type) {
    case SOURCE_SAME_SPECIES: interpolate(out->type, sizeof out->type, t->same_species, name(e->species, ud)); break;
    case SOURCE_GACHA_LEGENDARY:
      snprintf(out->type, sizeof out->type, "%s (%s)", t->gacha_legendary, name(gc_gacha_featured_species(e->timestamp_ms), ud));
      break;
    case SOURCE_GACHA_SHINY: snprintf(out->type, sizeof out->type, "%s", t->gacha_shiny); break;
    case SOURCE_GACHA_MOVE: snprintf(out->type, sizeof out->type, "%s", t->gacha_move); break;
    case SOURCE_EVENT: snprintf(out->type, sizeof out->type, "%s", t->event_type); break;
    default: break;
  }
}

/* ------------------------------------------------------------------ the list's state */

static bool egl_set_cursor(void* ud, int cursor) {
  egl_state* s = (egl_state*)ud;
  /* UiHandler.setCursor, then EggListUiHandler.setCursor: when it moved, show the egg under it */
  if (s->cursor == cursor) return false;
  s->cursor = cursor;
  s->detail = cursor + sg_item_offset(&s->grid);
  return true;
}

static void egl_update_detail(void* ud, int index) { ((egl_state*)ud)->detail = index; }

void egl_open(egl_state* s, int n) {
  static const scroll_grid_hooks hooks = {egl_set_cursor, NULL, egl_update_detail};
  s->n = n;
  s->cursor = -1;
  s->detail = 0;
  sg_init(&s->grid, EGL_ROWS, EGL_COLS, &hooks, s);
  sg_set_total(&s->grid, n); /* setTotalElements, then updateEggIcons (the cells are read by egl_cell_egg), then setCursor(0) */
  egl_set_cursor(s, 0);
}

bool egl_input(egl_state* s, sg_dir d) { return sg_input(&s->grid, d); }

int egl_cell_count(const egl_state* s) { return s->n < EGL_ICONS ? s->n : EGL_ICONS; }

int egl_cell_egg(const egl_state* s, int cell) {
  const int offset = sg_item_offset(&s->grid);
  const int shown = egl_cell_count(s) < s->n - offset ? egl_cell_count(s) : s->n - offset;
  return cell >= 0 && cell < shown ? cell + offset : -1;
}

bool egl_select_cell(egl_state* s, int cell) {
  if (egl_cell_egg(s, cell) < 0) return false;
  s->grid.cursor = cell;
  return egl_set_cursor(s, cell);
}
