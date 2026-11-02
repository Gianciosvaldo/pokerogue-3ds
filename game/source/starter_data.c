/* What the starter select screen gets from the game (see starter_data.h). */
#include "starter_data.h"

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "gc/data.h"
#include "i18n.h"
#include "pr_data.h"
#include "pr_enums.h"

const pr_species* menu_species(uint16_t id);

/* DexAttr (src/enums/dex-attr.ts) as game-core keeps it (gc_mon_dex_attr) */
#define DEX_NON_SHINY 1ull
#define DEX_SHINY 2ull
#define DEX_MALE 4ull
#define DEX_FEMALE 8ull
#define DEX_VARIANT_1 16ull
#define DEX_VARIANT_2 32ull
#define DEX_VARIANT_3 64ull
#define DEX_FORM_0 128ull /* DEFAULT_FORM: form f is bit 7 + f */
/* gc_starter_entry.preference_flags bits for the form and gender (the others are the nature, appearance and ability) */
#define PREF_FORM 8u
#define PREF_GENDER 16u
/* AbilityAttr and PassiveAttr */
#define ABILITY_1 1u
#define ABILITY_2 2u
#define ABILITY_HIDDEN 4u
#define PASSIVE_UNLOCKED 1u
#define PASSIVE_ENABLED 2u

bool ss_has_pokerus(uint16_t species){
  static uint64_t last=UINT64_MAX;static uint16_t ids[3];
  time_t now=time(NULL);uint64_t ms=now>0?(uint64_t)now*1000:0;
  if(ms/10800000!=last){last=ms/10800000;gc_pokerus_starters(ms,ids);}
  return species==ids[0]||species==ids[1]||species==ids[2];
}

/* how a species is picked with no stored preference */
typedef struct {
  const pr_species* sp;
  const pr_form* form;
  uint8_t form_index, variant, ability_index, nature;
  bool shiny, female, passive;
} props;

static bool caught(const gc_system* sys, uint16_t species) {
  if (!sys || species == 0 || species > GC_MAX_SPECIES_ID) return false;
  const uint64_t a = sys->dex[species].caught_attr;
  /* filterStarters: isNonShinyCaught, or a shiny with one of the three variants */
  return (a & DEX_NON_SHINY) || ((a & DEX_SHINY) && (a & (DEX_VARIANT_1 | DEX_VARIANT_2 | DEX_VARIANT_3)));
}

/* getSpeciesDefaultDexAttrProps, getStarterDefaultAbilityIndex and getStarterDefaultNature */
static bool defaults(const gc_system* sys, uint16_t species, props* p) {
  memset(p, 0, sizeof *p);
  p->sp = menu_species(species);
  if (!p->sp || p->sp->nforms < 1 || gc_starter_cost(species) < 0) return false;
  p->form = &pr_forms[p->sp->form0];
  if (!sys || species > GC_MAX_SPECIES_ID) return true;

  const uint64_t attr = sys->dex[species].caught_attr;
  const gc_starter_entry* pref = &sys->starters[species];
  p->female = p->sp->male_permille == 0; /* female only for a species that is always female */
  /* a stored gender is kept only while its bit is caught (initStarterPrefs :680-686) */
  if ((pref->preference_flags & PREF_GENDER) && (attr & (pref->gender_preference ? DEX_FEMALE : DEX_MALE))) p->female = pref->gender_preference != 0;
  p->shiny = (attr & DEX_SHINY) != 0;
  if (attr & DEX_VARIANT_3)
    p->variant = 2;
  else if (attr & DEX_VARIANT_2)
    p->variant = 1;
  /* a stored form is kept only while it is one the starter select offers and its bit is caught (initStarterPrefs :707-715) */
  const bool form_pref = (pref->preference_flags & PREF_FORM) && pref->form_preference < p->sp->nforms && pref->form_preference < 57 &&
                         ((gc_species_starter_forms(species) >> pref->form_preference) & 1) && (attr & (DEX_FORM_0 << pref->form_preference));
  if (form_pref) {
    p->form_index = pref->form_preference;
    p->form = &pr_forms[p->sp->form0 + p->form_index];
  }
  /* upstream picks form 0. A species whose form 0 was never caught (only a later form was) would be refused by
   * gc_starter_is_available, so it takes the first form that was caught. */
  else if (!(attr & DEX_FORM_0)) {
    for (int f = 1; f < p->sp->nforms && f < 57; f++) {
      if (attr & (DEX_FORM_0 << f)) {
        p->form_index = (uint8_t)f;
        p->form = &pr_forms[p->sp->form0 + f];
        break;
      }
    }
  }

  const uint32_t abilities = sys->starters[species].ability_attr;
  if (abilities & ABILITY_1)
    p->ability_index = 0;
  else if (!p->form->ability2 || (abilities & ABILITY_2))
    p->ability_index = 1;
  else
    p->ability_index = 2;

  const uint32_t natures = sys->dex[species].nature_attr;
  for (int n = 0; n < 25; n++) {
    if (natures & (1u << (n + 1))) {
      p->nature = (uint8_t)n;
      break;
    }
  }
  /* PassiveAttr.UNLOCKED | ENABLED */
  p->passive = sys->starters[species].passive_attr == (PASSIVE_UNLOCKED | PASSIVE_ENABLED);
  if ((pref->preference_flags&1) && pref->nature_preference<25 &&
      (natures&(1u<<(pref->nature_preference+1)))) p->nature=pref->nature_preference;
  if ((pref->preference_flags&2) && pref->appearance_preference<=3) {
    int a=pref->appearance_preference;
    if (!a && (attr&DEX_NON_SHINY)) {p->shiny=false;p->variant=0;}
    else if (a && (attr&DEX_SHINY) && (attr&(DEX_VARIANT_1<<(a-1)))) {
      p->shiny=true;p->variant=(uint8_t)(a-1);
    }
  }
  if ((pref->preference_flags&4) && pref->ability_preference<3) {
    int a=pref->ability_preference;
    bool unlocked=a==0 ? (abilities&ABILITY_1)!=0 : a==1 ? (abilities&ABILITY_2)!=0 : (abilities&ABILITY_HIDDEN)!=0;
    int ability=a==0?p->form->ability1:a==1?p->form->ability2:p->form->ability_hidden;
    if (unlocked && ability && !(a==1 && p->form->ability1==p->form->ability2 && (abilities&ABILITY_1)))
      p->ability_index=(uint8_t)a;
  }
  return true;
}

/* ---------------------------------------------------------------- cycling the form and the gender
 * The cycle buttons of starter-select-ui-handler.ts (updateCanCycle :2846, processCycleButtonsInput :1151, setNewFormIndex :1313,
 * setNewGender :1330) over the preferences game-core stores for a starter (preference_flags 8 and 16). */

/* the preferences as upstream holds them: each one is set or undefined */
typedef struct {
  bool form_set, female_set;
  uint8_t form;
  bool female;
} prefs;

static void prefs_load(const gc_starter_entry* e, prefs* p) {
  p->form_set = (e->preference_flags & PREF_FORM) != 0;
  p->form = e->form_preference;
  p->female_set = (e->preference_flags & PREF_GENDER) != 0;
  p->female = e->gender_preference != 0;
}

static void prefs_store(gc_starter_entry* e, const prefs* p) {
  e->preference_flags = (uint8_t)((e->preference_flags & ~(PREF_FORM | PREF_GENDER)) | (p->form_set ? PREF_FORM : 0) | (p->female_set ? PREF_GENDER : 0));
  e->form_preference = p->form_set ? p->form : 0;
  e->gender_preference = p->female_set && p->female ? 1 : 0;
}

/* species.forms.find(f => f.formKey === "female"): Meowstic, Indeedee, Basculegion and Oinkologne have their sexes as forms */
static bool has_female_form(const pr_species* sp) {
  for (int f = 0; f < sp->nforms; f++)
    if (!strcmp(pr_forms[sp->form0 + f].form_key, "female")) return true;
  return false;
}

static void set_gender(prefs* p, const pr_species* sp, bool female);
static void set_form(prefs* p, const pr_species* sp, int form) {
  p->form_set = true;
  p->form = (uint8_t)form;
  /* (upstream also sets the tera type to the form's first type: a starter keeps tera_type -1, which game-core resolves to the form's first type) */
  if (has_female_form(sp)) {
    const bool female = form == 1;
    if (!p->female_set || p->female != female) set_gender(p, sp, female);
  }
}
static void set_gender(prefs* p, const pr_species* sp, bool female) {
  p->female_set = true;
  p->female = female;
  if (has_female_form(sp)) {
    const int form = female ? 1 : 0;
    if (!p->form_set || p->form != form) set_form(p, sp, form);
  }
}

static bool form_selectable(uint16_t species, int f) { return f >= 0 && f < 57 && ((gc_species_starter_forms(species) >> f) & 1); }

/* getPokemonSpecies(id).forms.filter(isStarterSelectable || no form change leads anywhere) */
static bool has_form_change_key(uint16_t species) {
  for (int i = 0; i < gc_form_change_targets_count; i++)
    if (gc_form_change_targets_data[i].species == species && gc_form_change_targets_data[i].form_key[0]) return true;
  return false;
}

bool ss_can_cycle_gender(const gc_system* sys, uint16_t species) {
  if (!sys || species == 0 || species > GC_MAX_SPECIES_ID || gc_starter_cost(species) < 0) return false;
  const uint64_t a = sys->dex[species].caught_attr;
  return (a & DEX_MALE) && (a & DEX_FEMALE);
}

bool ss_can_cycle_form(const gc_system* sys, uint16_t species) {
  const pr_species* sp = menu_species(species);
  if (!sys || !sp || species > GC_MAX_SPECIES_ID || gc_starter_cost(species) < 0) return false;
  const uint64_t attr = sys->dex[species].caught_attr;
  /* upstream numbers the forms that pass the filter again from 0 before it asks which are caught (updateCanCycle :2882-2885): the
   * same here, so the button is offered exactly when upstream offers it */
  const bool keyed = has_form_change_key(species);
  int caught_count = 0, j = 0;
  for (int f = 0; f < sp->nforms; f++) {
    if (!form_selectable(species, f) && keyed) continue;
    if (j < 57 && (attr & (DEX_FORM_0 << j))) caught_count++;
    j++;
  }
  return caught_count > 1;
}

bool ss_form_offered(const gc_system* sys, uint16_t species, int form) {
  const pr_species* sp = menu_species(species);
  if (!sys || !sp || species > GC_MAX_SPECIES_ID || form < 0 || form >= sp->nforms || form >= 57) return false;
  return form_selectable(species, form) && (sys->dex[species].caught_attr & (DEX_FORM_0 << form));
}

/* store the new preferences when the starter they make is one game-core accepts; else leave the entry as it was */
static bool store_prefs(gc_system* sys, uint16_t species, const prefs* p) {
  gc_starter_entry* e = &sys->starters[species];
  const gc_starter_entry before = *e;
  prefs_store(e, p);
  gc_starter s;
  if (ss_starter(sys, species, &s) && gc_starter_is_available(sys, &s, NULL, NULL)) return true;
  *e = before;
  return false;
}

bool ss_set_form(gc_system* sys, uint16_t species, int form) {
  const pr_species* sp = menu_species(species);
  if (!sys || !sp || species > GC_MAX_SPECIES_ID || gc_starter_cost(species) < 0 || !ss_form_offered(sys, species, form)) return false;
  prefs p;
  prefs_load(&sys->starters[species], &p);
  set_form(&p, sp, form);
  return store_prefs(sys, species, &p);
}

bool ss_set_gender(gc_system* sys, uint16_t species, bool female) {
  const pr_species* sp = menu_species(species);
  if (!sys || !sp || species > GC_MAX_SPECIES_ID || gc_starter_cost(species) < 0 || !ss_can_cycle_gender(sys, species)) return false;
  prefs p;
  prefs_load(&sys->starters[species], &p);
  set_gender(&p, sp, female);
  return store_prefs(sys, species, &p);
}

bool ss_cycle_form(gc_system* sys, uint16_t species) {
  props now;
  if (!ss_can_cycle_form(sys, species) || !defaults(sys, species, &now)) return false;
  const int n = now.sp->nforms;
  int next = now.form_index;
  do {
    next = (next + 1) % n;
    if (ss_form_offered(sys, species, next)) break;
  } while (next != now.form_index);
  prefs p;
  prefs_load(&sys->starters[species], &p);
  set_form(&p, now.sp, next);
  return store_prefs(sys, species, &p);
}

bool ss_cycle_gender(gc_system* sys, uint16_t species) {
  props now;
  if (!ss_can_cycle_gender(sys, species) || !defaults(sys, species, &now)) return false;
  prefs p;
  prefs_load(&sys->starters[species], &p);
  set_gender(&p, now.sp, !(p.female_set && p.female)); /* setNewGender(species, !starterPreferences.female) */
  return store_prefs(sys, species, &p);
}

int ss_list(const gc_system* sys, sm_entry* out, int cap, int* refused) {
  int n = 0;
  if (refused) *refused = 0;
  if (!sys) return 0;
  for (int i = 0; i < gc_starter_cost_count; i++) {
    const uint16_t id = gc_starter_costs[i].id;
    props p;
    if (!caught(sys, id) || !defaults(sys, id, &p)) continue;
    /* What upstream lists, game-core must also be able to start: a species whose defaults gc_starter_is_available refuses would be
     * one the player sees and cannot begin with. The one known case: a prevolution caught only through an evolved form (a Litleo
     * after a female Pyroar) has no form bit in its dex entry, which upstream lets through with form 0 and game-core refuses. */
    gc_starter s;
    if (!ss_starter(sys, id, &s) || !gc_starter_is_available(sys, &s, NULL, NULL)) {
      if (refused) (*refused)++;
      continue;
    }
    if (n >= cap) break;
    sm_entry* e = &out[n++];
    memset(e, 0, sizeof *e);
    e->species = id;
    e->pokerus=ss_has_pokerus(id);
    e->cost = gc_starter_value(sys, id);
    e->base_cost = gc_starter_cost(id);
    e->form = p.form_index;
    e->variant = p.variant;
    e->female = p.female;
    e->shiny = p.shiny;
    e->passive = sys->starters[id].passive_attr != 0;
    e->hidden_ability = (sys->starters[id].ability_attr & ABILITY_HIDDEN) != 0;
    e->won = sys->starters[id].classic_win_count > 0;
  }
  return n; /* gc_starter_costs is sorted by species id */
}

void ss_describe(const gc_system* sys, uint16_t species, starter_info* out) {
  memset(out, 0, sizeof *out);
  props p;
  if (!defaults(sys, species, &p)) return;
  menu_mon* m = &out->mon;
  m->species = species;
  m->pokerus=ss_has_pokerus(species);
  m->form = p.form_index;
  m->gender = p.sp->male_permille < 0 ? MG_GENDERLESS : (p.female ? MG_FEMALE : MG_MALE);
  m->shiny = p.shiny;
  m->variant = p.variant;
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(species));
  m->types[0] = p.form->type1;
  m->types[1] = p.form->type2;
  m->ability = p.ability_index == 0 ? p.form->ability1 : p.ability_index == 1 ? p.form->ability2 : p.form->ability_hidden;
  m->passive = p.form->passive;
  m->has_passive = p.form->passive != 0;
  m->nature = p.nature;
  out->ability_hidden = p.ability_index == 2;
  if (sys && species <= GC_MAX_SPECIES_ID) {
    const uint32_t pa = sys->starters[species].passive_attr;
    out->passive_unlocked = (pa & PASSIVE_UNLOCKED) != 0;
    out->passive_enabled = (pa & PASSIVE_ENABLED) != 0;
  }
  gc_starter selected;
  if(ss_starter(sys,species,&selected)) memcpy(out->moves,selected.moves,sizeof out->moves);
}

bool ss_starter(const gc_system* sys, uint16_t species, gc_starter* out) {
  props p;
  memset(out, 0, sizeof *out);
  if (!defaults(sys, species, &p)) return false;
  out->species = species;
  out->form = p.form_index;
  out->ability_index = p.ability_index;
  out->nature = p.nature;
  out->female = p.female;
  out->shiny = p.shiny;
  out->variant = p.variant;
  out->passive = p.passive;
  out->pokerus = ss_has_pokerus(species);
  out->tera_type = -1; /* the species' first type */
  if (sys && species <= GC_MAX_SPECIES_ID) memcpy(out->ivs, sys->dex[species].ivs, sizeof out->ivs);
  uint16_t pool[64];int n=gc_starter_moves(sys,species,p.form_index,pool,64),w=0;
  if(n<0) return false;
  if(sys) for(int i=0;i<4;i++) {
    uint16_t move=sys->starters[species].moveset[i];bool legal=false,duplicate=false;
    for(int k=0;k<n;k++) if(pool[k]==move) legal=true;
    for(int k=0;k<w;k++) if(out->moves[k]==move) duplicate=true;
    if(legal && !duplicate) out->moves[w++]=move;
  }
  for(int i=0;i<n && w<4;i++) {
    bool duplicate=false;for(int k=0;k<w;k++) if(out->moves[k]==pool[i]) duplicate=true;
    if(!duplicate) out->moves[w++]=pool[i];
  }
  return true;
}

bool ss_team(const gc_system* sys, const uint16_t* species, int n, gc_starter* out, const char** why) {
  const char* dummy;
  if (!why) why = &dummy;
  gc_starter t[6];
  if (n < 1 || n > 6) {
    *why = "starterSelectUiHandler:invalidParty";
    return false;
  }
  double total = 0;
  for (int i = 0; i < n; i++) {
    double cost = 0;
    if (!ss_starter(sys, species[i], &t[i])) {
      *why = "not a starter species";
      return false;
    }
    if (!gc_starter_is_available(sys, &t[i], &cost, why)) return false;
    for (int k = 0; k < i; k++) {
      if (t[k].species == t[i].species) {
        *why = "the same species twice";
        return false;
      }
    }
    total += cost;
  }
  if (total > (double)gc_starter_budget(GAME_MODES_CLASSIC) + 1e-9) {
    *why = "starterSelectUiHandler:cost";
    return false;
  }
  memcpy(out, t, (size_t)n * sizeof t[0]);
  return true;
}
