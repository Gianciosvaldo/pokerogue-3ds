/* bgm_choose: which music upstream plays where (see bgm_choose.h). */
#include "bgm_choose.h"

#include <stdio.h>
#include <string.h>

#include "gm_enum_names.h"
#include "pr_data.h"
#include "pr_enums.h"

const gm_trainer_bgm* bgm_trainer_of(int trainer_type) {
  /* gm_trainer_bgms is sorted by TrainerType */
  int lo = 0, hi = gm_trainer_bgm_count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (gm_trainer_bgms[mid].trainer_type == trainer_type) return &gm_trainer_bgms[mid];
    if (gm_trainer_bgms[mid].trainer_type < trainer_type)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return NULL;
}

/* The same lookups as pr_pokemon_species / pr_pokemon_form (battle-core/src/pokemon.c): the species table is sorted by id, and a form
 * the species does not have is its first */
static const pr_species* species_of(int id) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == id) return &pr_species_table[mid];
    if (pr_species_table[mid].id < id)
      lo = mid + 1;
    else
      hi = mid - 1;
  }
  return NULL;
}

typedef struct {
  const pr_species* sp;
  int species, form;
} mon_t;

static bool form_is(const mon_t* p, const char* key) {
  if (!p->sp) return false;
  const pr_form* f = &pr_forms[p->sp->form0 + (p->form < p->sp->nforms ? p->form : 0)];
  return f->form_key && !strcmp(f->form_key, key);
}

/* species.legendary || subLegendary || mythical, or a Paradox Pokemon away from the End (Battle.getBgmOverride:280) */
static bool legendary_music(const mon_t* p, int biome, bool* legendary) {
  const pr_species* sp = p->sp;
  *legendary = sp && (sp->rarity & 2); /* rarity: 1 sub-legendary, 2 legendary, 4 mythical */
  if (sp && sp->rarity) return true;
  if (biome == BIOME_ID_END) return false;
  for (int i = 0; i < gm_paradox_species_count; i++)
    if (gm_paradox_species[i] == p->species) return true;
  return false;
}

/* The legendary Pokemon's battle music with the default (all generations) preference, Battle.getBgmOverride:310 */
static const char* legendary_all_gens(const mon_t* p, bool legendary) {
  switch (p->species) {
    case SPECIES_ID_ARTICUNO:
    case SPECIES_ID_ZAPDOS:
    case SPECIES_ID_MOLTRES:
    case SPECIES_ID_MEWTWO:
    case SPECIES_ID_MEW:
      return "battle_legendary_kanto";
    case SPECIES_ID_RAIKOU:
      return "battle_legendary_raikou";
    case SPECIES_ID_ENTEI:
      return "battle_legendary_entei";
    case SPECIES_ID_SUICUNE:
      return "battle_legendary_suicune";
    case SPECIES_ID_LUGIA:
      return "battle_legendary_lugia";
    case SPECIES_ID_HO_OH:
      return "battle_legendary_ho_oh";
    case SPECIES_ID_REGIROCK:
    case SPECIES_ID_REGICE:
    case SPECIES_ID_REGISTEEL:
    case SPECIES_ID_REGIGIGAS:
    case SPECIES_ID_REGIDRAGO:
    case SPECIES_ID_REGIELEKI:
      return "battle_legendary_regis_g6";
    case SPECIES_ID_GROUDON:
    case SPECIES_ID_KYOGRE:
      if (form_is(p, "primal")) return "battle_legendary_gro_kyo";
      return "battle_legendary_rayquaza";
    case SPECIES_ID_RAYQUAZA:
      return "battle_legendary_rayquaza";
    case SPECIES_ID_DEOXYS:
      return "battle_legendary_deoxys";
    case SPECIES_ID_UXIE:
    case SPECIES_ID_MESPRIT:
    case SPECIES_ID_AZELF:
      return "battle_legendary_lake_trio";
    case SPECIES_ID_HEATRAN:
    case SPECIES_ID_CRESSELIA:
    case SPECIES_ID_DARKRAI:
    case SPECIES_ID_SHAYMIN:
      return "battle_legendary_sinnoh";
    case SPECIES_ID_DIALGA:
    case SPECIES_ID_PALKIA:
      if (form_is(p, "origin")) return "battle_legendary_origin_forme";
      return "battle_legendary_dia_pal";
    case SPECIES_ID_GIRATINA:
      return "battle_legendary_giratina";
    case SPECIES_ID_ARCEUS:
      return "battle_legendary_arceus";
    case SPECIES_ID_COBALION:
    case SPECIES_ID_TERRAKION:
    case SPECIES_ID_VIRIZION:
    case SPECIES_ID_KELDEO:
    case SPECIES_ID_TORNADUS:
    case SPECIES_ID_LANDORUS:
    case SPECIES_ID_THUNDURUS:
    case SPECIES_ID_MELOETTA:
    case SPECIES_ID_GENESECT:
      return "battle_legendary_unova";
    case SPECIES_ID_KYUREM:
      return "battle_legendary_kyurem";
    case SPECIES_ID_XERNEAS:
    case SPECIES_ID_YVELTAL:
    case SPECIES_ID_ZYGARDE:
      return "battle_legendary_xern_yvel";
    case SPECIES_ID_TAPU_KOKO:
    case SPECIES_ID_TAPU_LELE:
    case SPECIES_ID_TAPU_BULU:
    case SPECIES_ID_TAPU_FINI:
      return "battle_legendary_tapu";
    case SPECIES_ID_COSMOG:
    case SPECIES_ID_COSMOEM:
    case SPECIES_ID_SOLGALEO:
    case SPECIES_ID_LUNALA:
      return "battle_legendary_sol_lun";
    case SPECIES_ID_NECROZMA:
      if (form_is(p, "dusk-mane") || form_is(p, "dawn-wings")) return "battle_legendary_dusk_dawn";
      if (form_is(p, "ultra")) return "battle_legendary_ultra_nec";
      return "battle_legendary_sol_lun";
    case SPECIES_ID_NIHILEGO:
    case SPECIES_ID_PHEROMOSA:
    case SPECIES_ID_BUZZWOLE:
    case SPECIES_ID_XURKITREE:
    case SPECIES_ID_CELESTEELA:
    case SPECIES_ID_KARTANA:
    case SPECIES_ID_GUZZLORD:
    case SPECIES_ID_POIPOLE:
    case SPECIES_ID_NAGANADEL:
    case SPECIES_ID_STAKATAKA:
    case SPECIES_ID_BLACEPHALON:
      return "battle_legendary_ub";
    case SPECIES_ID_ZACIAN:
    case SPECIES_ID_ZAMAZENTA:
      return "battle_legendary_zac_zam";
    case SPECIES_ID_ETERNATUS:
      if (form_is(p, "eternamax")) return "battle_legendary_eternatus_p2";
      return "battle_legendary_eternatus_p1";
    case SPECIES_ID_GLASTRIER:
    case SPECIES_ID_SPECTRIER:
      return "battle_legendary_glas_spec";
    case SPECIES_ID_CALYREX:
      if (form_is(p, "ice") || form_is(p, "shadow")) return "battle_legendary_riders";
      return "battle_legendary_calyrex";
    case SPECIES_ID_GALAR_ARTICUNO:
    case SPECIES_ID_GALAR_ZAPDOS:
    case SPECIES_ID_GALAR_MOLTRES:
      return "battle_legendary_birds_galar";
    case SPECIES_ID_WO_CHIEN:
    case SPECIES_ID_CHIEN_PAO:
    case SPECIES_ID_TING_LU:
    case SPECIES_ID_CHI_YU:
      return "battle_legendary_ruinous";
    case SPECIES_ID_GREAT_TUSK:
    case SPECIES_ID_SCREAM_TAIL:
    case SPECIES_ID_BRUTE_BONNET:
    case SPECIES_ID_FLUTTER_MANE:
    case SPECIES_ID_SLITHER_WING:
    case SPECIES_ID_SANDY_SHOCKS:
    case SPECIES_ID_IRON_TREADS:
    case SPECIES_ID_IRON_BUNDLE:
    case SPECIES_ID_IRON_HANDS:
    case SPECIES_ID_IRON_JUGULIS:
    case SPECIES_ID_IRON_MOTH:
    case SPECIES_ID_IRON_THORNS:
    case SPECIES_ID_ROARING_MOON:
    case SPECIES_ID_IRON_VALIANT:
    case SPECIES_ID_WALKING_WAKE:
    case SPECIES_ID_IRON_LEAVES:
    case SPECIES_ID_GOUGING_FIRE:
    case SPECIES_ID_RAGING_BOLT:
    case SPECIES_ID_IRON_BOULDER:
    case SPECIES_ID_IRON_CROWN:
    case SPECIES_ID_KORAIDON:
    case SPECIES_ID_MIRAIDON:
      return "battle_legendary_kor_mir";
    case SPECIES_ID_OKIDOGI:
    case SPECIES_ID_MUNKIDORI:
    case SPECIES_ID_FEZANDIPITI:
      return "battle_legendary_loyal_three";
    case SPECIES_ID_OGERPON:
      return "battle_legendary_ogerpon";
    case SPECIES_ID_TERAPAGOS:
      return "battle_legendary_terapagos";
    case SPECIES_ID_PECHARUNT:
      return "battle_legendary_pecharunt";
    default:
      if (legendary) return "battle_legendary_res_zek";
      return "battle_legendary_unova";
  }
}

/* ... and with the "gen 5" preference, Battle.getBgmOverride:286 */
static const char* legendary_gen_five(const mon_t* p, bool legendary) {
  switch (p->species) {
    case SPECIES_ID_ARTICUNO:
    case SPECIES_ID_ZAPDOS:
    case SPECIES_ID_MOLTRES:
    case SPECIES_ID_MEWTWO:
    case SPECIES_ID_MEW:
      return "battle_legendary_mew";
    case SPECIES_ID_REGIROCK:
    case SPECIES_ID_REGICE:
    case SPECIES_ID_REGISTEEL:
    case SPECIES_ID_REGIGIGAS:
    case SPECIES_ID_REGIDRAGO:
    case SPECIES_ID_REGIELEKI:
      return "battle_legendary_regis_g5";
    case SPECIES_ID_KYUREM:
      return "battle_legendary_kyurem";
    default:
      if (legendary) return "battle_legendary_res_zek";
      return "battle_legendary_unova";
  }
}

const char* bgm_battle_override(const bgm_scene* s, char buf[BGM_NAME_LEN]) {
  if (s->battle_type == BATTLE_TYPE_MYSTERY_ENCOUNTER && s->me_default) return NULL; /* the encounter sets its own music */
  if (s->battle_type == BATTLE_TYPE_TRAINER) {
    const gm_trainer_bgm* t = bgm_trainer_of(s->trainer_type);
    if (!t) return NULL;
    const int v = s->trainer_variant >= 0 && s->trainer_variant < 3 ? s->trainer_variant : 0;
    if (!s->started && t->has_encounter && (t->has_messages & (1 << v)) && t->encounter[v]) {
      snprintf(buf, BGM_NAME_LEN, "encounter_%s", t->encounter[v]);
      return buf;
    }
    return s->all_gens ? t->mixed : t->battle;
  }
  if (s->wave >= 191 && s->wave <= 194) return "end";
  if (s->wave >= 196 && s->wave <= 199) return "end_summit";
  for (int i = 0; i < s->n_enemy; i++) {
    const mon_t m = {species_of(s->enemy[i].species), s->enemy[i].species, s->enemy[i].form};
    const mon_t* p = &m;
    if (s->final_boss) return form_is(p, "eternamax") ? "battle_final" : "battle_final_encounter";
    bool legendary;
    if (legendary_music(p, s->biome, &legendary)) return s->all_gens ? legendary_all_gens(p, legendary) : legendary_gen_five(p, legendary);
  }
  if (s->wave <= 4) return "battle_wild";
  return NULL;
}

const char* bgm_arena_track(int biome, char buf[BGM_NAME_LEN]) {
  /* getBiomeKey: the enum member's name, lower case; no biome has a bgm of its own name in the pinned data */
  if (!gm_enum_lower(GM_E_BIOME_ID, biome, buf, BGM_NAME_LEN) || !buf[0]) snprintf(buf, BGM_NAME_LEN, "plains");
  return buf;
}

const char* bgm_scene_track(const bgm_scene* s, char buf[BGM_NAME_LEN]) {
  const char* o = bgm_battle_override(s, buf);
  return o ? o : bgm_arena_track(s->biome, buf);
}

const char* bgm_trainer_victory(int trainer_type) {
  const gm_trainer_bgm* t = bgm_trainer_of(trainer_type);
  return t && t->victory ? t->victory : "bw/victory_trainer"; /* TrainerConfig's default */
}

const char* bgm_title_track(int month, int day) { return month == 11 || (month == 0 && day <= 15) ? "winter_title" : "title"; }
