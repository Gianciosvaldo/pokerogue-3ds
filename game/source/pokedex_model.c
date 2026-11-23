/* What the Pokedex list shows (see pokedex_model.h). */
#include "pokedex_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"
#include "gc/gacha.h"
#include "gc/run.h"
#include "gc/system.h"
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
  pdf_reset(&s->filter);
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

/* ------------------------------------------------------------------ the filter */

void pdf_reset(pdf_filter* f) {
  memset(f, 0, sizeof *f);
  f->gens = PDF_GENS_ALL;
  f->types = PDF_TYPES_ALL;
  f->caught = PDF_C_ALL;
  f->sort = PDF_SORT_NUMBER;
}

int pdf_active(const pdf_filter* f) {
  int n = f->gens != PDF_GENS_ALL;
  n += f->types != PDF_TYPES_ALL;
  n += f->caught != PDF_C_ALL;
  n += f->passive != PDF_OFF;
  n += f->reduction != PDF_OFF;
  n += f->starter != PDF_OFF;
  n += f->favorite != PDF_OFF;
  n += f->win != PDF_OFF;
  n += f->hidden_ability != PDF_OFF;
  n += f->seen != PDF_OFF;
  n += f->encountered != PDF_OFF;
  n += f->egg != PDF_OFF;
  n += f->pokerus != PDF_OFF;
  n += f->sort != PDF_SORT_NUMBER || f->descending;
  return n;
}

/* Object.hasOwn(speciesEggMoves, id): the starters the Exclude states of the unlock filters apply to. The table lists every starter; Pikachu's row is empty
 * (upstream has no key for it), so a row counts when it holds a move. */
static bool has_egg_moves(uint16_t species) {
  for (int i = 0; i < gc_starter_egg_moves_count; i++)
    if (gc_starter_egg_moves_data[i].species == species) {
      const uint16_t* m = gc_starter_egg_moves_data[i].egg_moves;
      return m[0] || m[1] || m[2] || m[3];
    }
  return false;
}

/* a tri-state against what a species is */
static bool tri(uint8_t state, bool is) { return state == PDF_ON ? is : state == PDF_EXCLUDE ? !is : true; }

bool pdf_fits(const pdx_state* s, const pdf_filter* f, uint16_t species) {
  const gc_system* sys = s->sys;
  const pr_species* sp = species_data(species);
  if (!sp || sp->nforms < 1 || species > GC_MAX_SPECIES_ID) return false;
  const pr_form* form = &pr_forms[sp->form0];
  const uint16_t starter = pdx_starter_of(species);
  const gc_dex_entry* dex = &sys->dex[species];
  const gc_starter_entry* st = &sys->starters[starter];

  /* generation and the type of either slot (species.isOfType); a column with nothing set filters nothing */
  const uint16_t gens = f->gens ? f->gens : PDF_GENS_ALL;
  if (sp->generation < 1 || sp->generation > 9 || !(gens & (1u << (sp->generation - 1)))) return false;
  const uint32_t types = f->types ? f->types : PDF_TYPES_ALL;
  bool type_ok = false;
  if (form->type1 >= 0 && form->type1 < 18 && (types & (1u << form->type1))) type_ok = true;
  if (form->type2 >= 0 && form->type2 < 18 && (types & (1u << form->type2))) type_ok = true;
  if (!type_ok) return false;

  /* caught: the highest tier caught decides, as upstream's chain does */
  const uint64_t a = pdx_caught_attr(sys, species);
  const bool normal = (a & 1ull) != 0, shiny = (a & 2ull) != 0;
  const bool v1 = shiny && (a & 16ull), v2 = shiny && (a & 32ull), v3 = shiny && (a & 64ull);
  const bool uncaught = !normal && !v1 && !v2 && !v3;
  const uint8_t cf = f->caught ? f->caught : PDF_C_ALL;
  bool caught_ok = false;
  if ((cf & PDF_C_SHINY3) && v3) caught_ok = true;
  if ((cf & PDF_C_SHINY2) && v2 && !v3) caught_ok = true;
  if ((cf & PDF_C_SHINY1) && v1 && !v2 && !v3) caught_ok = true;
  if ((cf & PDF_C_NORMAL) && normal && !v1 && !v2 && !v3) caught_ok = true;
  if ((cf & PDF_C_UNCAUGHT) && uncaught) caught_ok = true;
  if (!caught_ok) return false;

  const bool progressable = has_egg_moves(starter);

  /* passive: the starter's candies against the passive's cost, and not yet unlocked */
  const bool passive_unlocked = st->passive_attr > 0;
  const int unlock_cost = gc_system_starter_unlock_cost(starter);
  const bool passive_unlockable = unlock_cost >= 0 && st->candy_count >= unlock_cost && !(st->passive_attr & 1u);
  switch (f->passive) {
    case PDF_ON: if (!passive_unlocked) return false; break;
    case PDF_EXCLUDE: if (!(progressable && !passive_unlocked)) return false; break;
    case PDF_UNLOCKABLE: if (!passive_unlockable) return false; break;
    default: break;
  }

  /* cost reduction */
  const bool one = st->value_reduction == 1, two = st->value_reduction == 2;
  /* isValueReductionAvailable: the candies for the next reduction (allStarterCandyCosts), and fewer than two bought; a purchase also needs a catch, this does not */
  static const uint8_t reduction_costs[10][2] = {{25, 60}, {25, 60}, {20, 50}, {15, 40}, {12, 35}, {10, 30}, {8, 20}, {5, 15}, {5, 15}, {5, 15}};
  const int base_cost = gc_starter_cost(starter);
  const bool reduction_unlockable = base_cost >= 1 && base_cost <= 10 && st->value_reduction < 2 && st->candy_count >= reduction_costs[base_cost - 1][st->value_reduction];
  switch (f->reduction) {
    case PDF_ON: if (!(one || two)) return false; break;
    case PDF_ONE: if (!one) return false; break;
    case PDF_TWO: if (!two) return false; break;
    case PDF_EXCLUDE: if (!(progressable && !(one || two))) return false; break;
    case PDF_UNLOCKABLE: if (!reduction_unlockable) return false; break;
    default: break;
  }

  /* misc */
  if (!tri(f->starter, gc_starter_cost(species) >= 0)) return false;
  if (!tri(f->favorite, false)) return false; /* the app has no favourites */
  if (!tri(f->win, st->classic_win_count > 0)) return false;
  /* species.abilityHidden !== ability1 && !== NONE */
  const bool species_has_ha = form->ability_hidden && form->ability_hidden != form->ability1;
  const bool has_ha = (st->ability_attr & 4u) != 0;
  if (f->hidden_ability == PDF_ON && !has_ha) return false;
  if (f->hidden_ability == PDF_EXCLUDE && !(species_has_ha && !has_ha)) return false;
  if (!tri(f->seen, dex->seen_attr != 0 || dex->caught_attr != 0)) return false;
  if (!tri(f->encountered, dex->seen_count != 0)) return false;
  const int egg_cost = gc_system_species_egg_cost(sys, starter);
  const bool egg = egg_cost >= 0 && st->candy_count >= egg_cost;
  if (f->egg == PDF_ON && !egg) return false;
  if (f->egg == PDF_EXCLUDE && !(progressable && !egg)) return false;
  const bool pokerus = s->pokerus && s->pokerus(species);
  if (!tri(f->pokerus, pokerus)) return false;
  return true;
}

/* the sort: the comparison, ascending; ties keep the order the species came in (Array.sort is stable: that is the registry's order, the dex table's) */
static const pdx_state* g_sort_state;
typedef struct {
  uint16_t id;
  int pos;
} sort_item;

static double iv_mean(uint16_t id) {
  int t = 0;
  for (int i = 0; i < 6; i++) t += g_sort_state->sys->dex[id].ivs[i];
  return t / 6.0;
}

static int by_key(const void* pa, const void* pb) {
  const sort_item *ia = pa, *ib = pb;
  const uint16_t a = ia->id, b = ib->id;
  const pdx_state* s = g_sort_state;
  const gc_system* sys = s->sys;
  double d = 0;
  switch (s->filter.sort) {
    case PDF_SORT_NUMBER: d = (double)a - (double)b; break;
    case PDF_SORT_COST: d = gc_starter_value(sys, pdx_starter_of(a)) - gc_starter_value(sys, pdx_starter_of(b)); break;
    case PDF_SORT_CANDY: d = sys->starters[pdx_starter_of(a)].candy_count - sys->starters[pdx_starter_of(b)].candy_count; break;
    case PDF_SORT_IV: d = iv_mean(a) - iv_mean(b); break;
    case PDF_SORT_NAME:
      if (s->name) {
        char na[96];
        snprintf(na, sizeof na, "%s", s->name(a)); /* the name buffers may rotate: keep the first */
        d = strcmp(na, s->name(b));
      }
      break;
    case PDF_SORT_CAUGHT: d = (double)sys->dex[a].caught_count - (double)sys->dex[b].caught_count; break;
    case PDF_SORT_HATCHED: d = (double)sys->dex[pdx_starter_of(a)].hatched_count - (double)sys->dex[pdx_starter_of(b)].hatched_count; break;
    default: break;
  }
  if (d != 0) return (d < 0) != s->filter.descending ? -1 : 1;
  return ia->pos - ib->pos;
}

void pdx_apply_filter(pdx_state* s, const pdf_filter* f) {
  static sort_item items[PDX_MAX_SPECIES];
  s->filter = *f;
  int n = 0;
  for (int i = 0; i < gc_species_dex_info_count && n < PDX_MAX_SPECIES; i++) {
    const uint16_t id = gc_species_dex_info_data[i].species;
    if (pdf_fits(s, f, id)) {
      items[n].id = id;
      items[n].pos = n;
      n++;
    }
  }
  g_sort_state = s;
  qsort(items, (size_t)n, sizeof items[0], by_key);
  s->n = n;
  for (int i = 0; i < n; i++) s->ids[i] = items[i].id;
  s->scroll = 0;
  update_scroll(s);
}
