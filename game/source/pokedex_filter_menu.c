/* The Pokedex's filter menu (see pokedex_filter_menu.h). Its shape is the starter select's (starter_host.c). */
#include "pokedex_filter_menu.h"

#include <stdio.h>
#include <string.h>

#include "i18n.h"
#include "ui.h"
#include "ui_widgets.h"

enum { FM_TOP, FM_GEN, FM_TYPE, FM_CAUGHT };
enum {
  FR_GEN, FR_TYPE, FR_CAUGHT, FR_PASSIVE, FR_REDUCTION, FR_STARTER, FR_WIN, FR_HA, FR_SEEN, FR_ENCOUNTERED, FR_EGG, FR_POKERUS, FR_SORT, FR_ORDER, FR_RESET, FR_DONE
};
#define MAX_ROWS 32

static const char* const state_names[] = {"Any", "Yes", "No", "Bought once", "Bought twice", "Can unlock now"};
static const char* const sort_names[PDF_SORT_COUNT] = {"Number", "Cost", "Candies", "IVs", "Name", "Times caught", "Times hatched"};

static struct {
  pfm_args a;
  int mode;
  int count;
  char labels[MAX_ROWS][64];
  const char* options[MAX_ROWS];
  int values[MAX_ROWS];
} M;

const char* pfm_button_label(const pdf_filter* f, char* out, unsigned n) {
  const int k = pdf_active(f);
  if (k) snprintf(out, n, "Filter (%d)", k);
  else snprintf(out, n, "Filter");
  return out;
}

static void add(int value, const char* label) {
  if (M.count >= MAX_ROWS) return;
  const int i = M.count++;
  snprintf(M.labels[i], sizeof M.labels[i], "%s", label);
  M.options[i] = M.labels[i];
  M.values[i] = value;
}

/* a tri-state column: the states it offers, in the order a press steps through them */
static uint8_t step(uint8_t cur, const uint8_t* offered, int n) {
  for (int i = 0; i < n; i++)
    if (offered[i] == cur) return offered[(i + 1) % n];
  return offered[0];
}

static void show(int mode);

static void done(int row, void* arg) {
  (void)arg;
  const int mode = M.mode;
  if (row < 0 || row >= M.count) {
    if (mode == FM_TOP) return; /* cancelled: the menu is closed */
    show(FM_TOP);
    return;
  }
  pdf_filter* f = M.a.filter;
  const int v = M.values[row];
  static const uint8_t yes_no[] = {PDF_OFF, PDF_ON, PDF_EXCLUDE};
  static const uint8_t passive[] = {PDF_OFF, PDF_ON, PDF_UNLOCKABLE, PDF_EXCLUDE};
  static const uint8_t reduction[] = {PDF_OFF, PDF_ON, PDF_ONE, PDF_TWO, PDF_UNLOCKABLE, PDF_EXCLUDE};
  if (mode == FM_TOP) {
    switch (v) {
      case FR_GEN: show(FM_GEN); return;
      case FR_TYPE: show(FM_TYPE); return;
      case FR_CAUGHT: show(FM_CAUGHT); return;
      case FR_PASSIVE: f->passive = step(f->passive, passive, 4); break;
      case FR_REDUCTION: f->reduction = step(f->reduction, reduction, 6); break;
      case FR_STARTER: f->starter = step(f->starter, yes_no, 3); break;
      case FR_WIN: f->win = step(f->win, yes_no, 3); break;
      case FR_HA: f->hidden_ability = step(f->hidden_ability, yes_no, 3); break;
      case FR_SEEN: f->seen = step(f->seen, yes_no, 3); break;
      case FR_ENCOUNTERED: f->encountered = step(f->encountered, yes_no, 3); break;
      case FR_EGG: f->egg = step(f->egg, yes_no, 3); break;
      case FR_POKERUS: f->pokerus = step(f->pokerus, yes_no, 3); break;
      case FR_SORT: f->sort = (uint8_t)((f->sort + 1) % PDF_SORT_COUNT); break;
      case FR_ORDER: f->descending = !f->descending; break;
      case FR_RESET: {
        pdf_filter fresh;
        pdf_reset(&fresh);
        *f = fresh;
        break;
      }
      default: return; /* Done */
    }
    ui_play_select();
    if (M.a.on_change) M.a.on_change(M.a.arg);
    show(FM_TOP);
    return;
  }
  if (v < 0) {
    show(FM_TOP); /* Back */
    return;
  }
  /* a toggle in a multiple-choice column; an empty column would hide everything, so the last one stays on */
  if (mode == FM_GEN) {
    const uint16_t m = (uint16_t)(f->gens ^ (1u << v));
    if (m) f->gens = m;
  } else if (mode == FM_TYPE) {
    const uint32_t m = f->types ^ (1u << v);
    if (m) f->types = m;
  } else if (mode == FM_CAUGHT) {
    const uint8_t m = (uint8_t)(f->caught ^ v);
    if (m) f->caught = m;
  }
  ui_play_select();
  if (M.a.on_change) M.a.on_change(M.a.arg);
  show(mode);
}

static void show(int mode) {
  M.mode = mode;
  M.count = 0;
  const pdf_filter* f = M.a.filter;
  char label[96];
  if (mode == FM_TOP) {
    const struct {
      int row;
      const char* name;
      int on, all;
    } multi[] = {{FR_GEN, "Generation", __builtin_popcount(f->gens), 9}, {FR_TYPE, "Type", __builtin_popcount(f->types), 18}, {FR_CAUGHT, "Caught", __builtin_popcount(f->caught), 5}};
    for (int i = 0; i < 3; i++) {
      if (multi[i].on == multi[i].all) snprintf(label, sizeof label, "%s: all", multi[i].name);
      else snprintf(label, sizeof label, "%s: %d of %d", multi[i].name, multi[i].on, multi[i].all);
      add(multi[i].row, label);
    }
    const struct {
      int row;
      const char* name;
      uint8_t state;
    } tri[] = {{FR_PASSIVE, "Passive", f->passive},     {FR_REDUCTION, "Cost reduction", f->reduction}, {FR_STARTER, "Starter", f->starter},
               {FR_WIN, "Has won", f->win},            {FR_HA, "Hidden ability", f->hidden_ability},  {FR_SEEN, "Seen", f->seen},
               {FR_ENCOUNTERED, "Encountered", f->encountered}, {FR_EGG, "Egg affordable", f->egg},     {FR_POKERUS, "Pokerus", f->pokerus}};
    for (int i = 0; i < 9; i++) {
      /* the Exclude state reads as "locked" for the unlocks and "No" elsewhere */
      const char* st = state_names[tri[i].state];
      if (tri[i].state == PDF_EXCLUDE && (tri[i].row == FR_PASSIVE || tri[i].row == FR_REDUCTION)) st = "Locked";
      if (tri[i].state == PDF_ON && tri[i].row == FR_PASSIVE) st = "Unlocked";
      snprintf(label, sizeof label, "%s: %s", tri[i].name, st);
      add(tri[i].row, label);
    }
    snprintf(label, sizeof label, "Sort by: %s", sort_names[f->sort]);
    add(FR_SORT, label);
    snprintf(label, sizeof label, "Order: %s", f->descending ? "Descending" : "Ascending");
    add(FR_ORDER, label);
    add(FR_RESET, "Reset");
    add(FR_DONE, "Done");
  } else if (mode == FM_GEN) {
    for (int g = 0; g < 9; g++) {
      snprintf(label, sizeof label, "%s Gen %d", (f->gens >> g) & 1 ? "[x]" : "[ ]", g + 1);
      add(g, label);
    }
    add(-1, "Back");
  } else if (mode == FM_TYPE) {
    for (int t = 0; t < 18; t++) {
      snprintf(label, sizeof label, "%s %s", (f->types >> t) & 1 ? "[x]" : "[ ]", i18n_type_name(t));
      add(t, label);
    }
    add(-1, "Back");
  } else if (mode == FM_CAUGHT) {
    static const struct {
      int bit;
      const char* name;
    } c[] = {{PDF_C_SHINY3, "Epic shiny"}, {PDF_C_SHINY2, "Rare shiny"}, {PDF_C_SHINY1, "Shiny"}, {PDF_C_NORMAL, "Normal"}, {PDF_C_UNCAUGHT, "Not caught"}};
    for (int i = 0; i < 5; i++) {
      snprintf(label, sizeof label, "%s %s", f->caught & c[i].bit ? "[x]" : "[ ]", c[i].name);
      add(c[i].bit, label);
    }
    add(-1, "Back");
  }
  ui_push_options(M.options, M.count, true, done, NULL);
}

void pfm_open(const pfm_args* a) {
  memset(&M, 0, sizeof M);
  M.a = *a;
  show(FM_TOP);
}
