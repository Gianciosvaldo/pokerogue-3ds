/* What the Pokedex list shows (see pokedex_model.h). */
#include "pokedex_model.h"

#include <string.h>

#include "gc/data.h"
#include "gc/run.h"
#include "pr_data.h"

#define DEX_DEFAULT_FORM_SHIFT 7 /* DexAttr.DEFAULT_FORM = 1 << 7, form f is 1 << (7 + f) */

static const pr_species* species_data(uint16_t id) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == id) return &pr_species_table[mid];
    if (pr_species_table[mid].id < id) lo = mid + 1;
    else hi = mid - 1;
  }
  return NULL;
}

/* speciesDataRegistry.getStarter: a starter is its own, any other species has the root of its line (not getRootSpeciesId(true): Raichu's is Pichu) */
uint16_t pdx_starter_of(uint16_t species) {
  if (gc_starter_cost(species) >= 0) return species;
  const pr_species* d = species_data(species);
  return d && d->starter > 0 ? (uint16_t)d->starter : species;
}

uint64_t pdx_caught_attr(const gc_system* sys, uint16_t species) {
  const uint16_t starter = pdx_starter_of(species);
  return sys->dex[species].caught_attr & sys->dex[starter].caught_attr & gc_species_full_unlocks_data(species);
}

bool pdx_is_seen(const gc_system* sys, uint16_t species) {
  if (sys->dex[species].seen_attr) return true;
  return sys->dex[pdx_starter_of(species)].caught_attr != 0;
}

void pdx_open(pdx_state* s, const gc_system* sys) {
  memset(s, 0, sizeof *s);
  s->sys = sys;
  for (int i = 0; i < gc_species_dex_info_count && s->n < PDX_MAX_SPECIES; i++) s->ids[s->n++] = gc_species_dex_info_data[i].species;
  /* sorted by number: ascending species id (the dex table is, but the order is the rule, not the table's) */
  for (int i = 1; i < s->n; i++) {
    const uint16_t v = s->ids[i];
    int j = i - 1;
    while (j >= 0 && s->ids[j] > v) {
      s->ids[j + 1] = s->ids[j];
      j--;
    }
    s->ids[j + 1] = v;
  }
  /* the containers are made for species 1..81 and show the list from updateStarters on */
  for (int i = 0; i < PDX_CELLS; i++) s->cell_species[i] = (uint16_t)(i + 1);
  for (int i = 0; i < PDX_CELLS && i < s->n; i++) s->cell_species[i] = s->ids[i];
}

/* updateScroll: the visible cells take the species the list has there */
static void update_scroll(pdx_state* s) {
  const int first = s->scroll * PDX_COLS;
  for (int i = 0; i < PDX_CELLS; i++)
    if (first + i < s->n) s->cell_species[i] = s->ids[first + i];
}

bool pdx_cell_visible(const pdx_state* s, int cell) { return cell >= 0 && cell < PDX_CELLS && s->scroll * PDX_COLS + cell < s->n; }

bool pdx_select_cell(pdx_state* s, int cell) {
  if (!pdx_cell_visible(s, cell)) return false;
  s->cursor = cell;
  return true;
}

char pdx_cell_tint(const pdx_state* s, int cell) {
  const uint16_t sp = s->cell_species[cell];
  if (pdx_caught_attr(s->sys, sp)) return 'c';
  return pdx_is_seen(s->sys, sp) ? 'g' : 'b';
}

/* setCursor in the grid: the cursor stays inside the 81 containers */
static bool set_cursor(pdx_state* s, int cursor) {
  if (cursor > PDX_CELLS - 1) cursor = PDX_CELLS - 1;
  if (cursor < 0) cursor = 0;
  s->cursor = cursor;
  return true; /* the container always has a species: upstream answers true even when the cursor did not move */
}

bool pdx_input(pdx_state* s, pdx_button b, pdx_mode* mode) {
  *mode = PDX_GRID;
  const int n = s->n;
  const int num_rows = (n + PDX_COLS - 1) / PDX_COLS;
  const int first = s->scroll * PDX_COLS;
  const int current_row = (first + s->cursor) / PDX_COLS;
  switch (b) {
    case PDX_UP:
      if (current_row > 0) {
        if (s->scroll > 0 && current_row - s->scroll == 0) {
          s->scroll--;
          update_scroll(s);
          return set_cursor(s, s->cursor);
        }
        return set_cursor(s, s->cursor - PDX_COLS);
      }
      *mode = PDX_FILTER_BAR; /* UP from the first row goes to the filter bar */
      return true;
    case PDX_DOWN:
      if (current_row < num_rows - 1 && s->cursor + PDX_COLS < n) {
        if (current_row - s->scroll == PDX_ROWS - 1) {
          s->scroll++;
          update_scroll(s);
          return set_cursor(s, s->cursor);
        }
        return set_cursor(s, s->cursor + PDX_COLS);
      } else if (num_rows > 1) {
        s->scroll = 0;
        update_scroll(s);
        return set_cursor(s, s->cursor % PDX_COLS);
      }
      *mode = PDX_FILTER_BAR; /* DOWN from a single row goes to the filters */
      return true;
    case PDX_LEFT:
      if (s->cursor % PDX_COLS == 0) {
        *mode = PDX_FILTER_TEXT; /* LEFT from the left edge goes to the text filters */
        return true;
      }
      return set_cursor(s, s->cursor - 1);
    case PDX_RIGHT:
      if (s->cursor % PDX_COLS < (current_row < num_rows - 1 ? PDX_COLS - 1 : (n - 1) % PDX_COLS)) return set_cursor(s, s->cursor + 1);
      *mode = PDX_FILTER_TEXT; /* RIGHT from the right edge goes to the text filters too */
      return true;
  }
  return false;
}

/* getCurrentDexProps' form: the stored preference when it is a starter's and valid, else the first caught form (getFormIndex) */
static int current_form(const gc_system* sys, uint16_t species, uint64_t caught) {
  const gc_starter_entry* st = &sys->starters[species];
  const uint64_t own = sys->dex[species].caught_attr & gc_species_full_unlocks_data(species);
  if ((st->preference_flags & 8) && st->form_preference != 0 && st->form_preference < 56) {
    const int f = st->form_preference;
    if ((gc_species_starter_forms(species) >> f & 1u) && (own & (1ull << (DEX_DEFAULT_FORM_SHIFT + f)))) return f;
  }
  if (caught < (1ull << DEX_DEFAULT_FORM_SHIFT)) return 0;
  int f = 0;
  while (f < 56 && !(caught & (1ull << (DEX_DEFAULT_FORM_SHIFT + f)))) f++;
  return f < 56 ? f : 0;
}

void pdx_panel_of(const pdx_state* s, pdx_panel* out) {
  const gc_system* sys = s->sys;
  const uint16_t sp = s->cell_species[s->cursor];
  out->species = sp;
  out->dex_number = sp % 2000;
  out->name_shown = sys->dex[sp].seen_attr != 0 || sys->dex[sp].caught_attr != 0;
  out->type1 = out->type2 = -1;
  /* setSpeciesDetails: the types of the form it is shown in, when that form is caught or the species is seen */
  const uint64_t caught = pdx_caught_attr(sys, sp);
  const int form = caught ? current_form(sys, sp, caught) : 0;
  const bool form_caught = (caught & (1ull << (DEX_DEFAULT_FORM_SHIFT + form))) != 0;
  if (!form_caught && !pdx_is_seen(sys, sp)) return;
  const pr_species* d = species_data(sp);
  if (!d) return;
  const int f = form < d->nforms ? form : 0;
  out->type1 = pr_forms[d->form0 + f].type1;
  out->type2 = pr_forms[d->form0 + f].type2;
}
