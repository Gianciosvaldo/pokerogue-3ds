/* The starter filter and sort (see starter_filter.h). */
#include "starter_filter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gc/data.h"
#include "i18n.h"
#include "pr_data.h"
#include "starter_data.h"

const pr_species* menu_species(uint16_t id);

/* DexAttr as game-core keeps it (the same bits as starter_data.c) */
#define DEX_NON_SHINY 1ull
#define DEX_SHINY 2ull
#define DEX_VARIANT_1 16ull
#define DEX_VARIANT_2 32ull
#define DEX_VARIANT_3 64ull
#define ABILITY_HIDDEN 4u
#define PASSIVE_UNLOCKED 1u

void ssf_reset(ssf* f) {
  memset(f, 0, sizeof *f);
  f->gens = SSF_GENS_ALL;
  f->types = SSF_TYPES_ALL;
  f->caught = SSF_C_ALL;
  f->rarity = SSF_R_ALL;
  f->sort = SSF_SORT_NUMBER;
}

int ssf_active(const ssf* f) {
  int n = f->gens != SSF_GENS_ALL;
  n += f->types != SSF_TYPES_ALL;
  n += f->caught != SSF_C_ALL;
  n += f->rarity != SSF_R_ALL;
  n += f->passive != SSF_OFF;
  n += f->reduction != SSF_OFF;
  n += f->win != SSF_OFF;
  n += f->hidden_ability != SSF_OFF;
  n += f->egg != SSF_OFF;
  n += f->pokerus != SSF_OFF;
  n += f->sort != SSF_SORT_NUMBER || f->descending;
  return n;
}

/* Object.hasOwn(speciesEggMoves, id): upstream's "progressable" starters, the ones the Exclude states of the unlock filters apply to */
static bool has_egg_moves(uint16_t species) {
  for (int i = 0; i < gc_starter_egg_moves_count; i++)
    if (gc_starter_egg_moves_data[i].species == species) return true;
  return false;
}

bool ssf_fits(const gc_system* sys, const ssf* f, uint16_t species) {
  const pr_species* sp = menu_species(species);
  if (!sys || !sp || sp->nforms < 1 || species > GC_MAX_SPECIES_ID) return false;
  const pr_form* form = &pr_forms[sp->form0];
  const gc_dex_entry* dex = &sys->dex[species];
  const gc_starter_entry* st = &sys->starters[species];

  /* generation, and the type of either slot (species.isOfType) */
  if (sp->generation < 1 || sp->generation > 9 || !(f->gens & (1u << (sp->generation - 1)))) return false;
  bool type_ok = false;
  if (form->type1 >= 0 && form->type1 < 18 && (f->types & (1u << form->type1))) type_ok = true;
  if (form->type2 >= 0 && form->type2 < 18 && (f->types & (1u << form->type2))) type_ok = true;
  if (!type_ok) return false;

  /* rarity (beyond upstream): rarity 1 sub-legendary, 2 legendary, 4 mythical */
  const uint8_t rb = sp->rarity == 1 ? SSF_R_SUB_LEGENDARY : sp->rarity == 2 ? SSF_R_LEGENDARY : sp->rarity == 4 ? SSF_R_MYTHICAL : SSF_R_ORDINARY;
  if (!(f->rarity & rb)) return false;

  /* caught: the highest tier caught decides, as upstream's chain does */
  const uint64_t a = dex->caught_attr;
  const bool normal = (a & DEX_NON_SHINY) != 0, shiny = (a & DEX_SHINY) != 0;
  const bool v1 = shiny && (a & DEX_VARIANT_1), v2 = shiny && (a & DEX_VARIANT_2), v3 = shiny && (a & DEX_VARIANT_3);
  bool caught_ok = false;
  if ((f->caught & SSF_C_SHINY3) && v3) caught_ok = true;
  if ((f->caught & SSF_C_SHINY2) && v2 && !v3) caught_ok = true;
  if ((f->caught & SSF_C_SHINY1) && v1 && !v2 && !v3) caught_ok = true;
  if ((f->caught & SSF_C_NORMAL) && normal && !v1 && !v2 && !v3) caught_ok = true;
  if (!caught_ok) return false;

  const bool progressable = has_egg_moves(species);

  /* passive */
  const bool passive_unlocked = st->passive_attr > 0;
  const int unlock_cost = gc_system_starter_unlock_cost(species);
  const bool passive_unlockable = unlock_cost >= 0 && st->candy_count >= unlock_cost && !(st->passive_attr & PASSIVE_UNLOCKED);
  switch (f->passive) {
    case SSF_ON: if (!passive_unlocked) return false; break;
    case SSF_EXCLUDE: if (!(progressable && !passive_unlocked)) return false; break;
    case SSF_UNLOCKABLE: if (!passive_unlockable) return false; break;
    default: break;
  }

  /* cost reduction */
  const bool one = st->value_reduction == 1, two = st->value_reduction == 2;
  const int reduction_cost = gc_system_starter_reduction_cost(sys, species);
  const bool reduction_unlockable = reduction_cost >= 0 && st->candy_count >= reduction_cost;
  switch (f->reduction) {
    case SSF_ON: if (!(one || two)) return false; break;
    case SSF_ONE: if (!one) return false; break;
    case SSF_TWO: if (!two) return false; break;
    case SSF_EXCLUDE: if (!(progressable && !(one || two))) return false; break;
    case SSF_UNLOCKABLE: if (!reduction_unlockable) return false; break;
    default: break;
  }

  /* misc */
  const bool won = st->classic_win_count > 0;
  if (f->win == SSF_ON && !won) return false;
  if (f->win == SSF_EXCLUDE && won) return false;
  /* species.abilityHidden !== ability1 && !== NONE */
  const bool species_has_ha = form->ability_hidden && form->ability_hidden != form->ability1;
  const bool has_ha = (st->ability_attr & ABILITY_HIDDEN) != 0;
  if (f->hidden_ability == SSF_ON && !has_ha) return false;
  if (f->hidden_ability == SSF_EXCLUDE && !(species_has_ha && !has_ha)) return false;
  const int egg_cost = gc_system_species_egg_cost(sys, species);
  const bool egg = egg_cost >= 0 && st->candy_count >= egg_cost;
  if (f->egg == SSF_ON && !egg) return false;
  if (f->egg == SSF_EXCLUDE && !(progressable && !egg)) return false;
  const bool pokerus = ss_has_pokerus(species);
  if (f->pokerus == SSF_ON && !pokerus) return false;
  if (f->pokerus == SSF_EXCLUDE && pokerus) return false;
  return true;
}

/* sortStarterSpecies: the comparison, ascending; qsort is not stable, so ties fall back to the species number */
static const gc_system* g_sys;
static const ssf* g_filter;

static double iv_mean(uint16_t s) {
  int t = 0;
  for (int i = 0; i < 6; i++) t += g_sys->dex[s].ivs[i];
  return t / 6.0;
}

static int by_key(const void* pa, const void* pb) {
  const sm_entry *a = pa, *b = pb;
  double d = 0;
  switch (g_filter->sort) {
    case SSF_SORT_NUMBER: d = (double)a->species - (double)b->species; break;
    case SSF_SORT_COST: d = a->cost - b->cost; break;
    case SSF_SORT_CANDY: d = g_sys->starters[a->species].candy_count - g_sys->starters[b->species].candy_count; break;
    case SSF_SORT_IV: d = iv_mean(a->species) - iv_mean(b->species); break;
    case SSF_SORT_NAME: {
      char na[64];
      snprintf(na, sizeof na, "%s", i18n_species_name(a->species)); /* the name buffers rotate: keep the first */
      const int c = strcmp(na, i18n_species_name(b->species));
      d = c;
      break;
    }
    case SSF_SORT_CAUGHT: d = (double)g_sys->dex[a->species].caught_count - (double)g_sys->dex[b->species].caught_count; break;
    case SSF_SORT_HATCHED: d = (double)g_sys->dex[a->species].hatched_count - (double)g_sys->dex[b->species].hatched_count; break;
    default: break;
  }
  if (d != 0) return (d < 0) != g_filter->descending ? -1 : 1;
  return (int)a->species - (int)b->species;
}

int ssf_apply(const gc_system* sys, const ssf* f, const sm_entry* all, int n_all, sm_entry* out) {
  int n = 0;
  for (int i = 0; i < n_all; i++)
    if (ssf_fits(sys, f, all[i].species)) out[n++] = all[i];
  if (f->sort != SSF_SORT_NUMBER || f->descending) {
    g_sys = sys;
    g_filter = f;
    qsort(out, (size_t)n, sizeof *out, by_key);
  }
  return n;
}
