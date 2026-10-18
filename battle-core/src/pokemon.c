/* Ports of src/field/pokemon.ts: stats, types, stat stages, effective stats. */
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/text.h"
#include <math.h>
#include <string.h>

const pr_species *pr_pokemon_species(const pr_pokemon *p)
{
    /* Species ids are sparse; the table is sorted by id. */
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == p->species) return &pr_species_table[mid];
        if (pr_species_table[mid].id < p->species) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const pr_form *pr_pokemon_base_form(const pr_pokemon *p)
{
    const pr_species *s = pr_pokemon_species(p);
    if (!s) return &pr_forms[0];
    return &pr_forms[s->form0 + (p->form < s->nforms ? p->form : 0)];
}

const pr_form *pr_pokemon_form(const pr_pokemon *p)
{
    if (!p->transformed) return pr_pokemon_base_form(p);
    pr_pokemon identity = {0};
    identity.species = p->transform_species;
    identity.form = p->transform_form;
    return pr_pokemon_base_form(&identity);
}

int pr_get_gender(const pr_pokemon *p)
{
    return p->transformed ? p->transform_gender : p->gender;
}

/* Pokemon.getSpriteScale: Transform changes the image but getFormKey/isMax
 * still read the permanent forms. Mystery custom spriteScale is not modeled. */
double pr_get_sprite_scale(const pr_pokemon *p)
{
    const char *key = pr_pokemon_base_form(p)->form_key;
    if (pr_is_max(p)) return 1.5;
    static const char *const starmobiles[] = {"segin-starmobile", "schedar-starmobile", "navi-starmobile",
                                            "ruchbah-starmobile", "caph-starmobile"};
    for (unsigned i = 0; key && i < sizeof starmobiles / sizeof starmobiles[0]; i++)
        if (!strcmp(key, starmobiles[i])) return 1.5;
    return 1;
}

bool pr_can_transform_into(const pr_pokemon *user, const pr_pokemon *target)
{
    return user && target && !user->transformed && !target->transformed
        && !user->illusion.active && !target->illusion.active
        && !user->fusion_species && !target->fusion_species
        && !pr_get_battler_tag(target, BATTLER_TAG_TYPE_SUBSTITUTE);
}

/* Pokemon.getAbility: zero temporary IDs are ignored, custom NONE is retained.
 * Scene/day overrides are supplied by the owning caller, not generated here. */
uint16_t pr_get_ability(const pr_pokemon *p, bool ignore_override)
{
    if (!ignore_override && p->ability_override > 0) return (uint16_t)p->ability_override;
    if (!p->ability_lookup_known) return ignore_override ? p->base_ability : p->ability;
    if (p->lookup_ability_override > 0) return (uint16_t)p->lookup_ability_override;
    if (p->fusion_species) {
        if (p->fusion_custom_ability >= 0) return (uint16_t)p->fusion_custom_ability;
        const pr_form *form = pr_pokemon_fusion_form(p);
        if (!ignore_override && p->temporary_fusion_species) {
            pr_pokemon identity = {0};
            identity.species = p->temporary_fusion_species;
            identity.form = p->temporary_fusion_form;
            form = pr_pokemon_base_form(&identity);
        }
        if (!form || p->fusion_ability_index < 0) return p->base_ability;
        return p->fusion_ability_index == 0 ? form->ability1 : p->fusion_ability_index == 1 ? form->ability2 : form->ability_hidden;
    }
    if (p->custom_ability >= 0) return (uint16_t)p->custom_ability;
    if (p->daily_boss_ability >= 0) return (uint16_t)p->daily_boss_ability;
    const pr_form *form = ignore_override ? pr_pokemon_base_form(p) : pr_pokemon_form(p);
    uint16_t ability = p->ability_index == 0 ? form->ability1 : p->ability_index == 1 ? form->ability2 : form->ability_hidden;
    if (!ability) {
        const pr_species *species = pr_pokemon_species(p);
        ability = pr_forms[species->form0].ability1;
    }
    return ability;
}

void pr_refresh_ability(pr_pokemon *p)
{
    if (!p->ability_lookup_known) return;
    p->base_ability = pr_get_ability(p, true);
    p->ability = pr_get_ability(p, false);
}

/* Core state portion of Pokemon.changeForm. Scene assets, seen/caught records
 * and modifier inventory refresh remain with the owning game/phase caller. */
void pr_change_form_state(pr_battle *bt, pr_pokemon *p, int form)
{
    const pr_species *species = pr_pokemon_species(p);
    if (!species || form < 0 || form >= species->nforms) {
        pr_note_unported(bt, PR_FEAT_FORM_CHANGE);
        return;
    }
    p->form = (uint8_t)form;
    if (p->ability_lookup_known) {
        /* getAbilityCount returns two when there is no hidden ability, even
         * when ability2 itself is NONE. It reads the effective species form. */
        int count = pr_pokemon_form(p)->ability_hidden ? 3 : 2;
        if (p->ability_index >= count) p->ability_index = (int8_t)(count - 1);
        pr_refresh_ability(p);
    }
    if (p->terastallized && (pr_is_mega(p) || pr_is_max(p))) {
        p->terastallized = false;
        p->stellar_types_boosted = 0;
        pr_form_change(bt, p, PR_FCT_SpeciesFormChangeLapseTeraTrigger);
    }
    pr_update_passive(bt, p);
    pr_recalculate_stats(bt, p);
}

void pr_restore_transform(pr_pokemon *p)
{
    p->transformed = false;
    p->summon_moves_active=false;
    p->summon_moves_alias_mask=0;
    p->transform_species = p->transform_form = p->transform_gender = 0;
    memset(p->transform_moves, 0, sizeof p->transform_moves);
    memset(p->transform_pp_used, 0, sizeof p->transform_pp_used);
    memset(p->transform_pp_up, 0, sizeof p->transform_pp_up);
    memset(p->transform_max_pp_override, 0, sizeof p->transform_max_pp_override);
    p->transform_n_moves = 0;
}

/* Learning changes Pokemon.moveset, even while getMoveset() returns a temporary copy. */
void pr_set_original_move(pr_pokemon *p, int slot, uint16_t move)
{
    if (slot < 0 || slot >= 4 || !move || move >= PR_MOVE_COUNT) return;
    p->summon_moves_alias_mask &= (uint8_t)~(1u << slot);
    uint16_t *moves = p->moves;
    uint8_t *used = p->pp_used;
    int8_t *up = p->pp_up;
    int16_t *max = p->max_pp_override;
    uint8_t *n = &p->n_moves;
    for(int i=*n;i<slot;i++){moves[i]=0;used[i]=up[i]=0;max[i]=-1;}
    moves[slot] = move; used[slot] = up[slot] = 0; max[slot] = -1;
    if (slot >= *n) *n = (uint8_t)(slot + 1);
}

/* The C arrays represent object identity with a per-slot alias mask. */
void pr_sync_moveset_aliases(pr_pokemon *p)
{
    if (!PR_HAS_SUMMON_MOVES(p)) return;
    for (int i=0;i<4;i++) if (p->summon_moves_alias_mask & (1u<<i)) {
        p->moves[i]=p->transform_moves[i];
        p->pp_used[i]=p->transform_pp_used[i];
        p->pp_up[i]=p->transform_pp_up[i];
        p->max_pp_override[i]=p->transform_max_pp_override[i];
    }
}

/* PokemonMove.usePp on a linked permanent or summon-data slot. */
void pr_use_moveset_pp(pr_pokemon *p, int slot, bool temporary, int count)
{
    if(slot<0 || slot>=4)return;
    uint16_t *moves=temporary?p->transform_moves:p->moves;
    uint8_t *used=temporary?p->transform_pp_used:p->pp_used;
    int8_t *up=temporary?p->transform_pp_up:p->pp_up;
    int16_t *override=temporary?p->transform_max_pp_override:p->max_pp_override;
    int step=(int)floor(pr_moves[moves[slot]].pp/5.0);if(step<1)step=1;
    int max=override[slot]>0?override[slot]:pr_moves[moves[slot]].pp+up[slot]*step;
    int next=used[slot]+count;used[slot]=(uint8_t)(next>max?max:next);
    if(p->summon_moves_alias_mask & (1u<<slot)) {
        if(temporary)p->pp_used[slot]=used[slot];
        else p->transform_pp_used[slot]=used[slot];
    }
}

static void detach_move_refs(pr_engine *e, pr_pokemon *p, int slot, bool original, bool temporary)
{
    /* A queued phase retains its old PokemonMove object after array replacement. */
    if (!e) return;
    for (int i=0;i<PR_MAX_PHASES;i++) if (e->used[i]) {
        pr_phase *ph=&e->pool[i];
        if ((ph->type!=PH_MOVE && ph->type!=PH_MOVE_HEADER) || ph->pokemon!=p || ph->move.slot!=slot) continue;
        bool temp=ph->move.from_transform;
        if ((temp && !temporary) || (!temp && !original)) continue;
        ph->move.pp_used=(temp?p->transform_pp_used:p->pp_used)[slot];
        ph->move.pp_up=(temp?p->transform_pp_up:p->pp_up)[slot];
        ph->move.max_pp_override=(temp?p->transform_max_pp_override:p->max_pp_override)[slot];
        ph->move.slot=-1;
    }
}

void pr_detach_temporary_moves(pr_engine *e, pr_pokemon *p)
{
    for(int i=0;i<4;i++)detach_move_refs(e,p,i,false,true);
}

void pr_set_move(pr_battle *bt, pr_engine *e, pr_pokemon *p, int slot, uint16_t move)
{
    (void)bt;
    if (slot<0 || slot>=4 || !move || move>=PR_MOVE_COUNT) return;
    detach_move_refs(e,p,slot,true,true);
    pr_set_original_move(p,slot,move);
    if (PR_HAS_SUMMON_MOVES(p)) {
        p->transform_moves[slot]=move;
        p->transform_pp_used[slot]=p->transform_pp_up[slot]=0;
        p->transform_max_pp_override[slot]=-1;
        if (slot>=p->transform_n_moves)p->transform_n_moves=(uint8_t)(slot+1);
        p->summon_moves_alias_mask|=(uint8_t)(1u<<slot);
    }
}

bool pr_copy_last_move(pr_battle *bt, pr_engine *e, pr_pokemon *user,
                       const pr_pokemon *target, int move, bool permanent)
{
    if (!target || (permanent && user->hp<=0)) return false;
    bool unknown=false;
    const pr_turn_move *last=pr_last_non_virtual_move(bt,target,false,true,&unknown);
    if (unknown || !last || !last->move) return false;
    int slot=-1;
    for (int i=0;i<PR_N_MOVES(user);i++)if(PR_MOVES(user)[i]==move){slot=i;break;}
    if (slot<0) return false;
    int copied=last->move;
    if (permanent)pr_set_move(bt,e,user,slot,(uint16_t)copied);
    else {
        if (PR_HAS_SUMMON_MOVES(user)) detach_move_refs(e,user,slot,false,true);
        uint16_t moves[4];uint8_t used[4];int8_t up[4];int16_t max[4];
        int count=PR_N_MOVES(user);
        uint8_t aliases=PR_HAS_SUMMON_MOVES(user)?user->summon_moves_alias_mask:(uint8_t)((1u<<count)-1);
        memcpy(moves,PR_MOVES(user),sizeof moves);memcpy(used,PR_PP_USED(user),sizeof used);
        memcpy(up,PR_PP_UP(user),sizeof up);memcpy(max,PR_MAX_PP(user),sizeof max);
        memcpy(user->transform_moves,moves,sizeof moves);memcpy(user->transform_pp_used,used,sizeof used);
        memcpy(user->transform_pp_up,up,sizeof up);memcpy(user->transform_max_pp_override,max,sizeof max);
        user->transform_n_moves=(uint8_t)count;user->summon_moves_active=true;
        user->summon_moves_alias_mask=aliases & (uint8_t)~(1u<<slot);
        user->transform_moves[slot]=(uint16_t)copied;user->transform_pp_used[slot]=user->transform_pp_up[slot]=0;
        user->transform_max_pp_override[slot]=-1;
    }
    if(e)pr_queue_msg(bt,e,pr_msg2(permanent?"moveTriggers:sketchedMove":"moveTriggers:copiedMove",
                                  pr_arg_pokemon(bt,"pokemonName",user),pr_arg_move("moveName",copied)));
    return true;
}

const pr_species *pr_pokemon_fusion_species(const pr_pokemon *p)
{
    if (!p->fusion_species) return NULL;
    int lo = 0, hi = PR_SPECIES_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (pr_species_table[mid].id == p->fusion_species) return &pr_species_table[mid];
        if (pr_species_table[mid].id < p->fusion_species) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

const pr_form *pr_pokemon_fusion_form(const pr_pokemon *p)
{
    const pr_species *fs = pr_pokemon_fusion_species(p);
    if (!fs) return NULL;
    return &pr_forms[fs->form0 + (p->fusion_form < fs->nforms ? p->fusion_form : 0)];
}

/* calculateStats() with calculateBaseStats(). Challenge FLIP_STAT remains
 * separate; Wonder Guard reads the permanent ability and the passive. */
void pr_recalculate_stats(pr_battle *bt, pr_pokemon *p)
{
    int32_t hp = p->hp, last_max = p->stats[STAT_HP];
    pr_calculate_stats(bt, p);
    int32_t max = p->stats[STAT_HP];
    if (hp > max) p->hp = max;
    else p->hp = hp && last_max && max > last_max ? hp + (max - last_max) : hp;
}

void pr_calculate_stats(pr_battle *bt, pr_pokemon *p)
{
    const pr_form *f = pr_pokemon_base_form(p);
    const pr_form *ff = pr_pokemon_fusion_form(p);
    double base[6];
    for (int s = 0; s < 6; s++) base[s] = f->base_stats[s];
    /* Source applies total/flat effects to the primary species before averaging
     * with the fusion or halving an unfused Spliced Endless actor. */
    pr_apply_base_stat_before_fusion(bt, p, base);
    for (int s = 0; s < 6; s++) {
        if (ff) {
            base[s] = ceil((base[s] + ff->base_stats[s]) / 2.0);
        } else if (bt->game_mode == GAME_MODES_SPLICED_ENDLESS) {
            /* calculateBaseStats: an unfused Pokemon has halved base stats in spliced-only mode. */
            base[s] = ceil(base[s] / 2.0);
        }
    }
    pr_apply_base_stat_after_fusion(bt, p, base);
    for (int s = 0; s < 6; s++) {
        double v = floor((2 * base[s] + p->ivs[s]) * p->level * 0.01);
        if (s == STAT_HP) {
            v = v + p->level + 10;
            pr_apply_incrementing_stat_modifier(bt, p, s, &v);
            /* hasAbility(WONDER_GUARD, canApply false, ignoreOverride true): the permanent ability or the passive */
            if (p->base_ability == ABILITY_ID_WONDER_GUARD || p->passive == ABILITY_ID_WONDER_GUARD) v = 1;
            p->hp = (int32_t)v; /* fresh Pokemon: hp starts at max */
        } else {
            v += 5;
            double m = pr_nature_mult[p->nature][s];
            pr_project_nature_weight_modifier(bt, p, &m);
            if (m != 1) {
                v = (m > 1) ? ceil(v * m) : floor(v * m);
                if (v < 1) v = 1;
            }
            pr_apply_incrementing_stat_modifier(bt, p, s, &v);
        }
        if (v < 1) v = 1;
        p->stats[s] = (int32_t)v;
    }
}

double pr_get_stat(const pr_pokemon *p, int stat, bool bypass_summon)
{
    if (!bypass_summon && p->summon_stats[stat]) return p->summon_stats[stat];
    return p->stats[stat];
}

/* getTeraType(): special real species include the fusion, never the disguise. */
int pr_get_tera_type(const pr_pokemon *p)
{
    if (p->species == SPECIES_ID_TERAPAGOS || p->fusion_species == SPECIES_ID_TERAPAGOS) return POKEMON_TYPE_STELLAR;
    if (p->species == SPECIES_ID_OGERPON || p->fusion_species == SPECIES_ID_OGERPON) {
        static const int8_t t[8] = {POKEMON_TYPE_GRASS, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE, POKEMON_TYPE_ROCK,
                                    POKEMON_TYPE_GRASS, POKEMON_TYPE_WATER, POKEMON_TYPE_FIRE, POKEMON_TYPE_ROCK};
        int form = p->species == SPECIES_ID_OGERPON ? p->form : p->fusion_form;
        if (form < 8) return t[form];
    }
    if (p->species == SPECIES_ID_SHEDINJA || p->fusion_species == SPECIES_ID_SHEDINJA) return POKEMON_TYPE_BUG;
    return p->tera_type;
}

/* Identity subset of Pokemon.setIllusion: snapshot, never replace real stats or
 * ability. Save a local copy so setIllusion(self) is also well-defined. */
void pr_set_illusion(pr_pokemon *p, const pr_pokemon *source)
{
    pr_illusion value = {source->species, source->fusion_species, source->form,
                         source->fusion_form, source->gender, true};
    p->illusion = value;
}

void pr_set_illusion_species(pr_pokemon *p, uint16_t species, uint8_t form)
{
    pr_pokemon identity = {0}; identity.species = species; identity.form = form;
    const pr_species *sp = pr_pokemon_species(&identity);
    int gender = -1;
    if (sp && sp->male_permille >= 0)
        gender = (p->id % 256) * 3.90625 < sp->male_permille ? 0 : 1;
    p->illusion = (pr_illusion){species, 0, form, 0, (int8_t)gender, true};
}

/* PR_DEV_ILLUSION, after the source's commented-out draft (IllusionPreSummonAbAttr): a Pokemon with a
 * trainer (every player Pokemon too) takes the look of the last other party member allowed in battle; a
 * wild one a species from the game's callback. None when there is no other member, when that member or the
 * user is a Terastallized Ogerpon/Terapagos pairing, for a fusion (NoFusionAbilityAbAttr) or when one is
 * already active. The draft's canApply returned true only with an illusion already set, so it never ran. */
void pr_illusion_on_summon(pr_battle *bt, pr_pokemon *p)
{
    if (!(bt->deviations & PR_DEV_ILLUSION) || !p || p->illusion.active || p->fusion_species) return;
    if (!pr_has_ability_active(bt, p, ABILITY_ID_ILLUSION)) return;
    if (p->is_player || p->has_trainer) {
        const int side = p->is_player ? 0 : 1;
        const pr_pokemon *last = NULL;
        for (int i = 0; i < bt->n_party[side]; i++) {
            const pr_pokemon *q = bt->party[side][i];
            if (q && q != p && q->hp > 0) last = q;
        }
        if (!last) return;
        if ((last->species == SPECIES_ID_OGERPON || last->species == SPECIES_ID_TERAPAGOS) && (last->terastallized || p->terastallized))
            return;
        pr_set_illusion(p, last);
        return;
    }
    uint16_t species;
    uint8_t form;
    if (bt->illusion_species && bt->illusion_species(bt, p, bt->illusion_species_ctx, &species, &form) && species)
        pr_set_illusion_species(p, species, form);
}

/* PR_DEV_ILLUSION: PostDefendIllusionBreakAbAttr, a damaging hit ends it */
bool pr_illusion_on_hit(pr_battle *bt, pr_pokemon *p, int hit_result)
{
    if (!(bt->deviations & PR_DEV_ILLUSION) || !p->illusion.active) return false;
    bool damaging = hit_result == HIT_RESULT_EFFECTIVE || hit_result == HIT_RESULT_EXTREMELY_EFFECTIVE
                 || hit_result == HIT_RESULT_SUPER_EFFECTIVE || hit_result == HIT_RESULT_NOT_VERY_EFFECTIVE
                 || hit_result == HIT_RESULT_MOSTLY_INEFFECTIVE || hit_result == HIT_RESULT_ONE_HIT_KO;
    if (!damaging || !pr_has_ability_active(bt, p, ABILITY_ID_ILLUSION)) return false;
    return pr_break_illusion(p);
}

bool pr_shows_illusion(const pr_battle *bt, const pr_pokemon *p)
{
    return bt && (bt->deviations & PR_DEV_ILLUSION) && p->illusion.active;
}

const pr_form *pr_shown_form(const pr_battle *bt, const pr_pokemon *p)
{
    return pr_shows_illusion(bt, p) ? pr_apparent_form(p, true) : pr_pokemon_form(p);
}

bool pr_break_illusion(pr_pokemon *p)
{
    bool active = p->illusion.active;
    memset(&p->illusion, 0, sizeof p->illusion);
    return active;
}

const pr_form *pr_apparent_form(const pr_pokemon *p, bool use_illusion)
{
    if (p->transformed || !use_illusion || !p->illusion.active) return pr_pokemon_form(p);
    pr_pokemon identity = {0};
    identity.species = p->illusion.species; identity.form = p->illusion.form;
    return pr_pokemon_form(&identity);
}

const pr_form *pr_apparent_fusion_form(const pr_pokemon *p, bool use_illusion)
{
    if (!use_illusion || !p->illusion.active) return pr_pokemon_fusion_form(p);
    pr_pokemon identity = {0};
    identity.fusion_species = p->illusion.fusion_species;
    identity.fusion_form = p->illusion.fusion_form;
    return pr_pokemon_fusion_form(&identity);
}

int pr_apparent_gender(const pr_pokemon *p, bool use_illusion)
{
    return p->transformed ? pr_get_gender(p) : use_illusion && p->illusion.active ? p->illusion.gender : p->gender;
}

/* getTypes({}) : returns count, out gets up to 3 types. */
int pr_get_types(const pr_pokemon *p, int8_t out[PR_MAX_TYPES], unsigned flags)
{
    /* A Stellar tera reports [STELLAR] unless the caller asks for the original types. */
    int tera = pr_get_tera_type(p);
    bool use_tera = !((flags & PR_TYPES_ORIGINAL_IF_STELLAR) && tera == POKEMON_TYPE_STELLAR);
    if (!(flags & PR_TYPES_NO_TERA) && p->terastallized && use_tera) {
        out[0] = (int8_t)tera;
        return 1;
    }
    /* getBaseTypes: summonData.types when set (and not bypassed), else the form's two */
    int8_t base[PR_MAX_SUMMON_TYPES];
    int nb;
    bool illusion = (flags & PR_TYPES_USE_ILLUSION) && p->illusion.active;
    if (!(flags & PR_TYPES_BYPASS_SUMMON) && p->n_summon_types && !illusion) {
        nb = p->n_summon_types;
        memcpy(base, p->summon_types, (size_t)nb);
    } else {
        const pr_form *f = pr_apparent_form(p, illusion);
        const pr_form *ff = pr_apparent_fusion_form(p, illusion);
        int8_t type1 = f->type1;
        int8_t second_type = f->type2 < 0 ? POKEMON_TYPE_UNKNOWN : f->type2;
        if (ff) {
            int8_t fusion_type1 = ff->type1;
            int8_t fusion_type2 = ff->type2 < 0 ? POKEMON_TYPE_UNKNOWN : ff->type2;
            if (fusion_type2 != POKEMON_TYPE_UNKNOWN && fusion_type2 != type1) {
                second_type = fusion_type2;
            } else if (fusion_type1 != type1) {
                second_type = fusion_type1;
            }
        }
        base[0] = type1;
        base[1] = second_type;
        nb = 2;
    }
    /* new Set(base), then drop UNKNOWN when something else is present */
    int n = 0;
    for (int i = 0; i < nb; i++) {
        bool have = false;
        for (int k = 0; k < n; k++) have = have || out[k] == base[i];
        if (!have) out[n++] = base[i];
    }
    if (n > 1) {
        int k = 0;
        for (int i = 0; i < n; i++)
            if (out[i] != POKEMON_TYPE_UNKNOWN) out[k++] = out[i];
        n = k;
    }
    /* then the type Forest's Curse or Trick-or-Treat added, if the set lacks it */
    if (!(flags & PR_TYPES_IGNORE_THIRD) && p->added_type) {
        bool have = false;
        for (int i = 0; i < n; i++) have = have || out[i] == p->added_type;
        if (!have) out[n++] = p->added_type;
    }
    return n;
}

bool pr_set_summon_types(pr_battle *bt, pr_pokemon *p, const int8_t *types, int n)
{
    if (n > PR_MAX_SUMMON_TYPES) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        return false;
    }
    memcpy(p->summon_types, types, (size_t)n);
    p->n_summon_types = (uint8_t)n;
    return true;
}

bool pr_is_of_type(const pr_pokemon *p, int type)
{
    int8_t t[PR_MAX_TYPES];
    int n = pr_get_types(p, t, 0);
    for (int i = 0; i < n; i++)
        if (t[i] == type) return true;
    return false;
}

double pr_get_stat_stage_multiplier(pr_battle *bt, const pr_pokemon *p, int stat, const pr_pokemon *opponent,
                                    int move, bool ignore_opp_ability, bool is_critical, bool simulated,
                                    bool ignore_held_items)
{
    int stage = p->stat_stages[stat - 1];
    bool ignored = false;
    if (opponent) {
        if (is_critical) {
            if ((stat == STAT_ATK || stat == STAT_SPATK) && stage < 0) stage = 0;
            if ((stat == STAT_DEF || stat == STAT_SPDEF) && stage > 0) stage = 0;
        }
        if (!ignore_opp_ability) {
            pr_ab_params pp = {.pokemon = opponent, .move = -1, .simulated = simulated, .stat = stat, .flag = &ignored};
            pr_apply_ab_attrs(bt, PR_AC_IgnoreOpponentStatStagesAbAttr, &pp);
        }
        /* IgnoreOpponentStatStagesAttr (Chip Away, Sacred Sword...) sets the flag */
        if (move >= 0 && pr_move_has_attr((uint16_t)move, PR_AC_IgnoreOpponentStatStagesAttr)) ignored = true;
    }
    if (ignored) return 1;
    double m = (double)(2 + stage > 2 ? 2 + stage : 2) / (double)(2 - stage > 2 ? 2 - stage : 2);
    if (!ignore_held_items) pr_apply_temp_stat_stage_booster_modifiers(bt, p->is_player, stat, &m); /* X items */
    return m < 4 ? m : 4;
}

/* ECMAScript ToInt32 precedes the native signed shift modifiers. A C cast
 * outside int32 is not the JavaScript modulo-2^32 conversion. */
static int32_t effective_stat_int32(double value)
{
    if (!isfinite(value) || value == 0) return 0;
    double bits = fmod(trunc(value), 4294967296.0);
    if (bits < 0) bits += 4294967296.0;
    if (bits >= 2147483648.0) bits -= 4294967296.0;
    return (int32_t)bits;
}

double pr_get_effective_stat_p(pr_battle *bt, const pr_pokemon *p, int stat, const pr_stat_params *sp)
{
    double v = pr_get_stat(p, stat, false);
    if (!sp->ignore_held_items) pr_apply_stat_booster_modifiers(bt, p, stat, &v);

    /* Ruin abilities: getField(true) in field order, stop once one applied. Never ignored. */
    bool field_applied = false;
    for (int i = 0; i < 4 && !field_applied; i++) {
        const pr_pokemon *f = bt->field[i];
        if (!f || f->hp <= 0) continue;
        pr_ab_params fp = {.pokemon = f, .opponent = p, .move = -1, .simulated = sp->simulated, .stat = stat,
                           .value = &v, .flag = &field_applied};
        pr_apply_ab_attrs(bt, PR_AC_FieldMultiplyStatAbAttr, &fp);
    }

    /* move ?? allMoves[MoveId.NONE] */
    int m = sp->move >= 0 ? sp->move : MOVE_ID_NONE;
    pr_ab_params ap = {.pokemon = p, .move = m, .simulated = sp->simulated, .stat = stat, .value = &v};
    if (!sp->ignore_ability) pr_apply_ab_attrs(bt, PR_AC_StatMultiplierAbAttr, &ap);

    const pr_pokemon *ally = pr_get_ally(bt, p);
    if (ally) {
        pr_ab_params lp = ap;
        lp.pokemon = ally;
        lp.ignore_ability = (sp->move >= 0 && (pr_moves[sp->move].flags & MOVE_FLAGS_IGNORE_ABILITIES))
                            || sp->ignore_ally_ability;
        pr_apply_ab_attrs(bt, PR_AC_AllyStatMultiplierAbAttr, &lp);
    }

    int weather = !sp->for_defend || sp->ignore_opp_ability || !sp->opponent
                      ? bt->weather
                      : pr_get_effective_weather_for_move(bt, sp->opponent);

    double ret = v * pr_get_stat_stage_multiplier(bt, p, stat, sp->opponent, sp->move, sp->ignore_opp_ability,
                                                  sp->is_critical, sp->simulated, sp->ignore_held_items);
    switch (stat) {
    case STAT_ATK:
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_SLOW_START)) ret = (double)(effective_stat_int32(ret) >> 1);
        break;
    case STAT_DEF:
        if (pr_is_of_type(p, POKEMON_TYPE_ICE) && weather == WEATHER_TYPE_SNOW) ret *= 1.5;
        break;
    case STAT_SPDEF:
        if (pr_is_of_type(p, POKEMON_TYPE_ROCK) && weather == WEATHER_TYPE_SANDSTORM) ret *= 1.5;
        break;
    case STAT_SPD: {
        int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_TAILWIND, side)) ret *= 2;
        if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_GRASS_WATER_PLEDGE, side)) ret = (double)(effective_stat_int32(ret) >> 2);
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_SLOW_START)) ret = (double)(effective_stat_int32(ret) >> 1);
        if (p->status == STATUS_EFFECT_PARALYSIS) ret = (double)(effective_stat_int32(ret) >> 1);
        if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_UNBURDEN) && pr_has_ability_active(bt, p, ABILITY_ID_UNBURDEN)) ret *= 2;
        break;
    }
    }
    /* findTag(t => t instanceof HighestStatBoostTag && t.stat === stat): Protosynthesis, Quark Drive */
    for (int i = 0; i < p->n_tags; i++)
        if (pr_battler_tag_is(p->tags[i].type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && p->tags[i].stat == stat) {
            ret *= p->tags[i].multiplier;
            break;
        }
    ret = floor(ret);
    return ret > 1 ? ret : 1;
}

double pr_get_effective_stat(pr_battle *bt, const pr_pokemon *p, int stat,
                              const pr_pokemon *opponent, int move, bool is_critical, bool for_defend)
{
    pr_stat_params sp = {.opponent = opponent, .move = move, .is_critical = is_critical, .for_defend = for_defend,
                         .simulated = true};
    return pr_get_effective_stat_p(bt, p, stat, &sp);
}

/* isForciblyGrounded(): IGNORE_FLYING tag or Gravity */
bool pr_is_forcibly_grounded(pr_battle *bt, const pr_pokemon *p)
{
    return pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_IGNORE_FLYING) || pr_arena_has_tag(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH);
}

/* isForciblyUngrounded(): FLOATING / TELEKINESIS tags, then UngroundedAbAttr (Levitate) */
bool pr_is_forcibly_ungrounded(pr_battle *bt, const pr_pokemon *p)
{
    if (pr_is_forcibly_grounded(bt, p)) return false;
    if (pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_FLOATING) || pr_has_battler_tag(bt, p, BATTLER_TAG_TYPE_TELEKINESIS))
        return true;
    bool ungrounded = false;
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = false, .flag = &ungrounded}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_UngroundedAbAttr, &pp);
    return ungrounded;
}

/* isGrounded(ignoreSemiInvulnerable) */
bool pr_is_grounded(pr_battle *bt, const pr_pokemon *p, bool ignore_semi_invulnerable)
{
    if (pr_is_forcibly_grounded(bt, p)) return true;
    if (pr_is_forcibly_ungrounded(bt, p)) return false;
    bool semi = !ignore_semi_invulnerable && pr_has_battler_tag_class(bt, p, PR_TAGCLASS_SEMI_INVULNERABLE);
    int8_t t[PR_MAX_TYPES];
    int n = pr_get_types(p, t, PR_TYPES_ORIGINAL_IF_STELLAR);
    bool flying = false;
    for (int i = 0; i < n; i++) if (t[i] == POKEMON_TYPE_FLYING) flying = true;
    return !semi && !flying;
}

/* JS Math.round: halves go toward +infinity, and it is exact (no x + 0.5 rounding). */
double js_round(double x)
{
    double r = floor(x);
    return (x - r >= 0.5) ? r + 1 : r;
}

/* hasPassive() and getPassiveAbility(), stored in p->passive (0 = no passive). hasPassive is
 * Pokemon.passive or being a boss; the game-mode rules around it (no passive for the classic
 * final boss or endless bosses, the passives challenge, daily-run passives) and the test
 * overrides belong to the mode layer, which isn't ported. */
/* Pokemon.hasPassive(), with the game mode's rules when the mode is known: no passive for the
 * enemy in the classic final battle or on an endless boss wave, and a daily run's Pokemon has the
 * passive its custom data names. (The passives challenge isn't modelled.) */
bool pr_has_passive(pr_battle *bt, const pr_pokemon *p)
{
    int gm = bt->game_mode, w = bt->wave_index;
    if (gm == GAME_MODES_CHALLENGE) pr_note_unported(bt, PR_FEAT_WAVE);
    bool endless = gm == GAME_MODES_ENDLESS || gm == GAME_MODES_SPLICED_ENDLESS;
    if (!p->is_player && ((gm == GAME_MODES_CLASSIC && w == 200) || (endless && (w % 250 == 0 || w % 1000 == 0))))
        return false;
    if (gm == GAME_MODES_DAILY && p->custom_passive != -1) return true;
    return p->passive_flag || p->is_boss;
}

uint16_t pr_get_passive_ability(const pr_pokemon *p)
{
    return p->custom_passive >= 0 ? (uint16_t)p->custom_passive : pr_pokemon_base_form(p)->passive;
}

void pr_update_passive(pr_battle *bt, pr_pokemon *p)
{
    if (!pr_has_passive(bt, p)) { p->passive = 0; return; }
    p->passive = pr_get_passive_ability(p);
}

double pr_damage_indirect(pr_battle *bt, pr_pokemon *p, double damage)
{
    if (p->hp <= 0) return 0; /* isFainted() */
    /* Indirect damage sets preventEndure, so Endure, the Sturdy tag and Focus Band don't apply.
     * With ignoreSegments the HP loss is the same for a boss; what the cleared segments bring
     * (the segment index, random stat boosts drawn from the global RNG, their stat change) doesn't
     * change this damage calculation and is recorded for the turn engine. The classic final
     * boss's first-segment rule belongs to the mode layer. */
    if (p->is_boss) pr_note_deferred(bt, PR_FEAT_BOSS_SEGMENTS);
    double d = damage < p->hp ? damage : p->hp;
    p->hp -= (int32_t)d;
    pr_hook_ab(bt, p, PR_AC_PostDamageAbAttr); /* applyAbAttrs("PostDamageAbAttr"): no source */
    return d;
}

double pr_get_hp_ratio(const pr_pokemon *p, bool precise)
{
    double r = (double)p->hp / pr_get_stat(p, STAT_HP, true);
    return precise ? r : js_round(r * 100) / 100;
}

/* getWeight(): form weight minus 100 kg per Autotomize, then weight abilities, at least 0.1 */
double pr_get_weight(pr_battle *bt, const pr_pokemon *p)
{
    const pr_battler_tag *at = pr_find_battler_tag_class(p, PR_TAGCLASS_AUTOTOMIZED);
    double removed = at ? 100.0 * at->autotomize_count : 0;
    const pr_form *f = pr_pokemon_form(p);
    double w = f->weight_x10 / 10.0 - removed; /* weight_x10 / 10.0 is the same double as upstream's decimal literal */
    pr_ab_params pp = {.pokemon = p, .move = -1, .simulated = false, .value = &w}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_WeightMultiplierAbAttr, &pp);
    return w > 0.1 ? w : 0.1;
}

/* weather.ts getEffectiveWeatherForMove: an override (Cloud Nine-like attrs on the user)
 * wins over suppression. */
int pr_get_effective_weather_for_move(pr_battle *bt, const pr_pokemon *user)
{
    int w = WEATHER_TYPE_NONE;
    pr_ab_params pp = {.pokemon = user, .move = -1, .simulated = false, .holder = &w}; /* not simulated upstream */
    pr_apply_ab_attrs(bt, PR_AC_PreAttackWeatherOverrideAbAttr, &pp);
    if (w != WEATHER_TYPE_NONE) return w;
    if (bt->weather == WEATHER_TYPE_NONE || pr_weather_effect_suppressed(bt)) return WEATHER_TYPE_NONE;
    return bt->weather;
}

/* Task 53: Mega Evolution & Dynamax */
bool pr_is_mega(const pr_pokemon *p)
{
    if (!p) return false;
    const pr_form *f = pr_pokemon_base_form(p); /* isMega uses getFormKey(), ignoring Transform */
    if (f && f->form_key) {
        if (!strncmp(f->form_key, "mega", 4) || !strcmp(f->form_key, "primal"))
            return true;
    }
    const pr_form *ff = pr_pokemon_fusion_form(p);
    if (ff && ff->form_key) {
        if (!strncmp(ff->form_key, "mega", 4) || !strcmp(ff->form_key, "primal"))
            return true;
    }
    return false;
}

bool pr_is_max(const pr_pokemon *p)
{
    if (!p) return false;
    if (p->is_dynamaxed) return true;
    const pr_form *f = pr_pokemon_base_form(p); /* isMax uses getFormKey(), ignoring Transform */
    if (f && f->form_key) {
        if (!strncmp(f->form_key, "gigantamax", 10) || !strcmp(f->form_key, "eternamax"))
            return true;
    }
    const pr_form *ff = pr_pokemon_fusion_form(p);
    if (ff && ff->form_key) {
        if (!strncmp(ff->form_key, "gigantamax", 10) || !strcmp(ff->form_key, "eternamax"))
            return true;
    }
    return false;
}

void pr_mega_evolve(pr_battle *bt, pr_pokemon *p)
{
    if (!p) return;
    p->queued_mega = false;
    const pr_species *sp = pr_pokemon_species(p);
    if (!sp) return;

    int mega_form_idx = -1;
    for (uint16_t i = 0; i < sp->nforms; i++) {
        const pr_form *f = &pr_forms[sp->form0 + i];
        if (f->form_key && !strncmp(f->form_key, "mega", 4)) {
            mega_form_idx = (int)i;
            break;
        }
    }
    if (mega_form_idx < 0) return;

    pr_msg_arg pre_name_arg = pr_arg_pokemon(bt, "preName", p);
    pr_change_form_state(bt, p, mega_form_idx);

    const pr_form *mf = pr_pokemon_base_form(p);
    if (!p->ability_lookup_known) {
        p->base_ability = mf->ability1;
        if (p->ability_override < 0) p->ability = mf->ability1;
    }

    if (bt->engine) {
        pr_queue_msg(bt, bt->engine, pr_msg2("battlePokemonForm:megaChange", pre_name_arg, pr_arg_pokemon_name(bt, "pokemonName", p)));
    }

    pr_ab_params gain = {.pokemon = p, .move = -1, .only = 1, .on_gain = true};
    pr_apply_ab_attrs(bt, PR_AC_PostSummonAbAttr, &gain);
}

void pr_dynamax(pr_battle *bt, pr_pokemon *p)
{
    if (!p || p->is_dynamaxed) return;
    p->queued_dynamax = false;
    p->pre_dynamax_form = p->form;
    const pr_species *sp = pr_pokemon_species(p);

    if (sp) {
        for (uint16_t i = 0; i < sp->nforms; i++) {
            const pr_form *f = &pr_forms[sp->form0 + i];
            if (f->form_key && !strncmp(f->form_key, "gigantamax", 10)) {
                p->form = (uint8_t)i;
                break;
            }
        }
    }

    p->is_dynamaxed = true;
    p->dynamax_turns_left = 3;

    double mult = 1.5 + ((double)p->dynamax_level * 0.05);
    p->stats[STAT_HP] = (int32_t)js_round((double)p->stats[STAT_HP] * mult);
    p->hp = (int32_t)js_round((double)p->hp * mult);

    if (bt->engine) {
        const pr_form *f = pr_pokemon_base_form(p);
        if (f->form_key && strstr(f->form_key, "gigantamax")) {
            pr_msg_arg pre_name_arg = pr_arg_pokemon(bt, "preName", p);
            pr_queue_msg(bt, bt->engine, pr_msg1("battlePokemonForm:gigantamaxChange", pre_name_arg));
        }
    }
}
