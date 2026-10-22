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
#define DEX_VARIANT_1 16ull
#define DEX_VARIANT_2 32ull
#define DEX_VARIANT_3 64ull
#define DEX_FORM_0 128ull /* DEFAULT_FORM: form f is bit 7 + f */
/* AbilityAttr and PassiveAttr */
#define ABILITY_1 1u
#define ABILITY_2 2u
#define ABILITY_HIDDEN 4u
#define PASSIVE_UNLOCKED 1u
#define PASSIVE_ENABLED 2u

static bool has_pokerus(uint16_t species){
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
  p->female = p->sp->male_permille == 0; /* female only for a species that is always female */
  p->shiny = (attr & DEX_SHINY) != 0;
  if (attr & DEX_VARIANT_3)
    p->variant = 2;
  else if (attr & DEX_VARIANT_2)
    p->variant = 1;
  /* upstream picks form 0. A species whose form 0 was never caught (only a later form was) would be refused by
   * gc_starter_is_available, so it takes the first form that was caught. */
  if (!(attr & DEX_FORM_0)) {
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
  const gc_starter_entry* pref=&sys->starters[species];
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
    e->pokerus=has_pokerus(id);
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
  out->pokerus = has_pokerus(species);
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
