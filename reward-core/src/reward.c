/* The player's reward options: regenerateModifierPoolThresholds, getNewModifierTypeOption's player branch,
 * getModifierTypeOptionWithRetry, getPlayerModifierTypeOptions and the generators they call
 * (src/modifier/modifier-type.ts), and SelectModifierPhase.start / getModifierCount
 * (src/phases/select-modifier-phase.ts). The JS each step replaces is quoted where it happens. */
#include "pr/reward.h"
#include "reward_internal.h"
#include <math.h>
#include <string.h>

static int seed_int(pr_reward_ctx *ctx, int range) { return pr_rng_seed_int(&ctx->run->rng, range, 0); }

/* ---- lookups ---- */
const pr_reward_species *pr_reward_species_find(int species)
{
    int lo = 0, hi = pr_reward_species_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const int id = pr_reward_species_data[mid].id;
        if (id == species) return &pr_reward_species_data[mid];
        if (id < species) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* getFormKey() of a species in form `form`: forms[form].formKey, "" (string 1) with no forms or out of range */
int pr_reward_form_key(int species, int form)
{
    const pr_reward_species *s = pr_reward_species_find(species);
    if (!s) return 1;
    if (form == 0) {
        if (!s->n_forms) return 1;
        return pr_reward_forms[s->form_start].key;
    }
    if (form < 0 || form >= s->n_forms) return -1;
    return pr_reward_forms[s->form_start + form].key;
}

static const pr_gen_type *gen_type_find(pr_item_type t)
{
    int lo = 0, hi = pr_gen_type_count - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const pr_gen_type *g = &pr_gen_types[mid];
        if (g->item == t.item && g->arg == t.arg) return g;
        if (g->item < t.item || (g->item == t.item && g->arg < t.arg)) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

int pr_item_type_name(pr_item_type t)
{
    if (t.item >= PR_ITEM_COUNT_) return -1;
    if (!pr_items[t.item].gen) return pr_items[t.item].name;
    const pr_gen_type *g = gen_type_find(t);
    return g ? g->name : -1;
}

int pr_item_type_group(pr_item_type t)
{
    if (t.item >= PR_ITEM_COUNT_) return -1;
    if (!pr_items[t.item].gen) return pr_items[t.item].group;
    const pr_gen_type *g = gen_type_find(t);
    return g ? g->group : -1;
}

/* p.getHeldItems() has one of `item` (and `arg`, >= 0) */
const pr_owned_item *pr_reward_held(const pr_reward_ctx *ctx, int p, int item, int arg)
{
    for (int i = 0; i < ctx->n_items; i++) {
        const pr_owned_item *o = &ctx->items[i];
        if (o->owner == p && o->item == item && (arg < 0 || o->arg == arg)) return o;
    }
    return NULL;
}

bool pr_reward_has_item(const pr_reward_ctx *ctx, int item)
{
    for (int i = 0; i < ctx->n_items; i++)
        if (ctx->items[i].item == item) return true;
    return false;
}

/* SpeciesStatBoosterModifier held by p with the key `key` (either pool's id) */
static bool holds_species_stat(const pr_reward_ctx *ctx, int p, int key)
{
    return pr_reward_held(ctx, p, PR_ITEM_SPECIES_STAT_BOOSTER, key) || pr_reward_held(ctx, p, PR_ITEM_RARE_SPECIES_STAT_BOOSTER, key);
}

/* ---- generators (generateType(party) with no pregenArgs): the arg, or -2 for null ---- */
#define GEN_NULL (-2)

/* AttackTypeBoosterModifierTypeGenerator: the allowed members' attack moves' types, at most 3 each, in
 * first-seen order; randSeedInt(totalWeight) over them; none: null */
static int gen_attack_type(pr_reward_ctx *ctx)
{
    int8_t types[32];
    int weights[32], n = 0, total = 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (!m->allowed_in_challenge) continue;
        for (int i = 0; i < m->n_attack_types; i++) {
            const int t = m->attack_types[i];
            int k = 0;
            while (k < n && types[k] != t) k++;
            if (k == n) {
                if (n == 32) continue;
                types[n] = (int8_t)t;
                weights[n++] = 0;
            }
            if (weights[k] < 3) { weights[k]++; total++; }
        }
    }
    if (!n) return GEN_NULL;
    const int r = seed_int(ctx, total);
    int w = 0;
    for (int k = 0; k < n; k++) {
        if (r < w + weights[k]) return types[k];
        w += weights[k];
    }
    return GEN_NULL;
}

/* SpeciesStatBoosterModifierTypeGenerator(rare): weights per key of the rarity for each member of a matching
 * species (or fusion species) that doesn't hold that key; randSeedInt(totalWeight, 1), the first key whose
 * running total reaches it */
static const struct { uint16_t species[3]; uint8_t n; bool rare; } ssb[6] = {
    {{SPECIES_ID_PIKACHU}, 1, true},
    {{SPECIES_ID_CUBONE, SPECIES_ID_MAROWAK, SPECIES_ID_ALOLA_MAROWAK}, 3, true},
    {{SPECIES_ID_DITTO}, 1, true},
    {{SPECIES_ID_DITTO}, 1, true},
    {{SPECIES_ID_CLAMPERL}, 1, false},
    {{SPECIES_ID_CLAMPERL}, 1, false},
};
static bool ssb_has(int key, int species)
{
    for (int i = 0; i < ssb[key].n; i++)
        if (ssb[key].species[i] == species) return true;
    return false;
}
static int gen_species_stat(pr_reward_ctx *ctx, bool rare)
{
    int weights[6] = {0};
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        for (int k = 0; k < 6; k++) {
            if (ssb[k].rare != rare || holds_species_stat(ctx, p, k)) continue;
            /* hasFling is always false upstream (Fling isn't implemented) */
            if (ssb_has(k, m->species) || (m->fusion_species >= 0 && ssb_has(k, m->fusion_species))) weights[k]++;
        }
    }
    int total = 0;
    for (int k = 0; k < 6; k++) total += weights[k];
    if (!total) return GEN_NULL;
    const int r = pr_rng_seed_int(&ctx->run->rng, total, 1);
    int w = 0;
    for (int k = 0; k < 6; k++) {
        if (!weights[k]) continue;
        if (r <= w + weights[k]) return k;
        w += weights[k];
    }
    return GEN_NULL;
}

/* getTms(formKey) of a species form: its pr_tm_lists row, or -1 (a form index past the species' forms) */
static int tm_list(int species, int form)
{
    const pr_reward_species *s = pr_reward_species_find(species);
    if (!s) return -1;
    if (!s->n_forms) return pr_reward_forms[s->form_start].tms;
    if (form < 0 || form >= s->n_forms) return -1;
    return pr_reward_forms[s->form_start + form].tms;
}

static bool list_has(const uint16_t *v, int n, int x)
{
    for (int i = 0; i < n; i++)
        if (v[i] == x) return true;
    return false;
}

/* TmModifierTypeGenerator(tier):
 *   partyMemberCompatibleTms = party.map(p => p.getCompatibleTms(true, true, true));
 *   the flat list filtered to the tier and to implemented moves, first occurrences only; randSeedInt(length)
 * getCompatibleTms(excludeKnown, excludeLevelUp, excludeUsedTMs): a Set of species.getTms(getFormKey()), then the
 * fusion's, minus this.moveset, the level moves and usedTMs, in insertion order */
static int gen_tm(pr_reward_ctx *ctx, int tier)
{
    static uint16_t out[1024];
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        static uint16_t set[512];
        int ns = 0;
        for (int part = 0; part < 2; part++) {
            if (part && m->fusion_species < 0) break;
            const int li = part ? tm_list(m->fusion_species, m->fusion_form) : tm_list(m->species, m->form);
            if (li < 0) { ctx->run->unported |= PR_RUN_UNDEFINED_UPSTREAM; continue; }
            const pr_tm_list *l = &pr_tm_lists[li];
            for (unsigned i = 0; i < l->n; i++) {
                const int tm = pr_tm_pool[l->start + i];
                if (list_has(set, ns, tm)) continue;
                if (ns == 512) { ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS; break; }
                set[ns++] = (uint16_t)tm;
            }
        }
        /* applyChallenges(PLAYER_TM_COMPATIBILITY): the Moveset Randomizer shuffles the list (not ported) */
        if (ctx->challenges & (1u << CHALLENGES_MOVESET_RANDOMIZER)) ctx->run->unported |= PR_REWARD_UNPORTED_CHALLENGE;
        for (int i = 0; i < ns; i++) {
            const int tm = set[i];
            if (list_has(m->base_moves, m->n_base_moves, tm) || list_has(m->level_moves, m->n_level_moves, tm)
                || list_has(m->used_tms, m->n_used_tms, tm))
                continue;
            if (tm >= pr_tm_tier_count || pr_tm_tier[tm] < 0 || (pr_tm_tier[tm] & 7) != tier || (pr_tm_tier[tm] & 8))
                continue;
            if (list_has(out, n, tm)) continue;
            if (n == 1024) { ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS; break; }
            out[n++] = (uint16_t)tm;
        }
    }
    if (!n) return GEN_NULL;
    return out[seed_int(ctx, n)];
}

/* SpeciesEvolutionCondition.conditionsFulfilled(pokemon) (isValidItemEvolution passes no forFusion) */
bool pr_reward_evo_conditions(pr_reward_ctx *ctx, int p, const pr_item_evo *e)
{
    const pr_reward_mon *m = &ctx->party[p];
    for (int c = 0; c < e->n_cond; c++) {
        const pr_evo_cond *k = &pr_evo_conds[e->cond_start + c];
        const int16_t *list = &pr_evo_cond_lists[k->list_start];
        bool ok = false;
        switch (k->key) {
        case 1: ok = m->friendship >= k->a; break;                               /* FRIENDSHIP */
        case 2:                                                                  /* TIME */
            for (int i = 0; i < k->n_list; i++) ok |= list[i] == ctx->time_of_day;
            break;
        case 3: ok = list_has(m->base_moves, m->n_base_moves, k->a); break;      /* MOVE: this.moveset */
        case 4:                                                                  /* MOVE_TYPE */
            for (int i = 0; i < m->n_base_moves; i++) ok |= pr_moves[m->base_moves[i]].type == k->a;
            break;
        case 5:                                                                  /* PARTY_TYPE */
            for (int q = 0; q < ctx->n_party; q++)
                for (int i = 0; i < ctx->party[q].n_types; i++) ok |= ctx->party[q].types[i] == k->a;
            break;
        case 6:                                                                  /* WEATHER */
            for (int i = 0; i < k->n_list; i++) ok |= list[i] == ctx->weather;
            break;
        case 7:                                                                  /* BIOME */
            for (int i = 0; i < k->n_list; i++) ok |= list[i] == ctx->biome;
            break;
        case 8: {                                                                /* TYROGUE */
            /* getMoveset(true).find(m => tyrogueMoves.includes(m.moveId))?.moveId === cond.move */
            int found = -1;
            for (int i = 0; i < m->n_base_moves && found < 0; i++) {
                const int mv = m->base_moves[i];
                if (mv == MOVE_ID_LOW_SWEEP || mv == MOVE_ID_MACH_PUNCH || mv == MOVE_ID_RAPID_SPIN) found = mv;
            }
            ok = found == k->a;
            break;
        }
        case 9: ok = false; break;                                               /* SHEDINJA */
        case 10: {                                                               /* EVO_TREASURE_TRACKER */
            const pr_owned_item *o = pr_reward_held(ctx, p, PR_ITEM_EVOLUTION_TRACKER_GIMMIGHOUL, -1);
            ok = o && o->stack + m->persistent_treasure >= k->a;
            break;
        }
        case 11: {                                                               /* RANDOM_FORM */
            /* executeWithSeedOffset(() => ret = !randSeedInt(value), pokemon.id): the run seed shifted by the id
             * (fromCharCode wraps to 16 bits, so the id's low 16 bits do) */
            pr_rng r = pr_run_seed_offset_rng(ctx->run, (int)(m->id & 0xffff), NULL);
            ok = !pr_rng_seed_int(&r, k->a, 0);
            break;
        }
        case 12: ok = ctx->caught && ctx->caught(ctx->caught_user, k->a); break;  /* SPECIES_CAUGHT */
        case 13: ok = k->a == m->gender; break;                                   /* GENDER */
        case 14:                                                                 /* NATURE */
            for (int i = 0; i < k->n_list; i++) ok |= list[i] == m->nature;
            break;
        case 15: ok = holds_species_stat(ctx, p, k->a); break;                    /* HELD_ITEM */
        default: ok = false;
        }
        if (!ok) return false;
    }
    return true;
}

const pr_species_slice *pr_reward_slice_find(const pr_species_slice *v, int n, int species)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (v[mid].species == species) return &v[mid];
        if (v[mid].species < species) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

bool pr_reward_pause_exempt(int species)
{
    return species == SPECIES_ID_SLOWPOKE || species == SPECIES_ID_EEVEE || species == SPECIES_ID_KIRLIA ||
           species == SPECIES_ID_SNORUNT;
}

/* EvolutionItemModifierTypeGenerator(rare): the members' valid item evolutions (the species', then the fusions'),
 * their items with (item > 50) === rare; randSeedInt(length) */
static int gen_evolution_item(pr_reward_ctx *ctx, bool rare)
{
    uint16_t pool[64];
    int n = 0;
    for (int part = 0; part < 2; part++)
        for (int p = 0; p < ctx->n_party; p++) {
            const pr_reward_mon *m = &ctx->party[p];
            const int species = part ? m->fusion_species : m->species;
            if (species < 0) continue;
            const pr_reward_species *s = pr_reward_species_find(species);
            if (!s || !(s->flags & 1)) continue;                      /* hasEvolutions */
            if (m->pause_evolutions && !pr_reward_pause_exempt(species)) continue;
            const pr_species_slice *sl = pr_reward_slice_find(pr_item_evo_species, pr_item_evo_species_count, species);
            if (!sl) continue;
            /* isValidItemEvolution(p, forFusion): the level, the form (preFormKey null or the (fusion) form key),
             * the conditions */
            const int key = part ? pr_reward_form_key(m->fusion_species, m->fusion_form) : pr_reward_form_key(m->species, m->form);
            for (int i = 0; i < sl->n; i++) {
                const pr_item_evo *e = &pr_item_evos[sl->start + i];
                if (m->level < e->level) continue;
                if (e->pre_form_key >= 0 && e->pre_form_key != key) continue;
                if (!pr_reward_evo_conditions(ctx, p, e)) continue;
                if (!e->item || (e->item > 50) != rare) continue;
                if (n < 64) pool[n++] = e->item;
                else ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS;
            }
        }
    if (!n) return GEN_NULL;
    return pool[seed_int(ctx, n)];
}

/* A form change condition (SpeciesFormChangeCondition.predicate(p)) */
bool pr_reward_fc_condition(pr_reward_ctx *ctx, int p, const pr_fc_cond *c)
{
    const pr_reward_mon *m = &ctx->party[p];
    switch (c->kind) {
    case 1:   /* p.hasAbility(a): the ability or the passive, when it can apply (canApplyAbility: caller) */
        return (m->cur_ability == c->a && m->can_apply_ability) ||
               (m->passive && m->passive == c->a && m->can_apply_passive);
    case 2: return ctx->run->mode != GAME_MODES_DAILY;
    case 3: return pr_reward_hp_ratio(m) >= 0.5;
    case 4: return pr_reward_hp_ratio(m) < 0.5;
    case 5: return ctx->caught && ctx->caught(ctx->caught_user, c->a);
    }
    return false;
}

/* FormChangeItemModifierTypeGenerator(isRare): for each member whose species has form changes, the item triggers of
 * its form changes that pass (Mega/Primal need the Mega Bracelet, G-Max/Eternamax the Dynamax Band; no conditions or
 * one that holds; preFormKey === getFormKey()), active, and whose item the member doesn't hold; Necrozma's Ultranecrozium
 * Z waits for a Lunarizer or Solarizer to be taken; unique triggers (a Set: two members of one species share them);
 * their items with (item && item < 150) === isRare; randSeedInt(length) */
static int gen_form_change_item(pr_reward_ctx *ctx, bool rare)
{
    uint16_t trig[128];   /* pr_item_fcs indexes, first occurrence order */
    int nt = 0;
    const bool mega = pr_reward_has_item(ctx, PR_ITEM_MEGA_BRACELET), gmax = pr_reward_has_item(ctx, PR_ITEM_DYNAMAX_BAND);
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        const pr_reward_species *s = pr_reward_species_find(m->species);
        if (!s || !(s->flags & 2)) continue;
        const pr_species_slice *sl = pr_reward_slice_find(pr_item_fc_species, pr_item_fc_species_count, m->species);
        if (!sl) continue;
        const int key = pr_reward_form_key(m->species, m->form);
        uint16_t mine[64];
        int nm = 0;
        for (int i = 0; i < sl->n; i++) {
            const pr_item_fc *fc = &pr_item_fcs[sl->start + i];
            if ((fc->flags & 1) && !mega) continue;
            if ((fc->flags & 2) && !gmax) continue;
            if (fc->n_cond) {
                bool any = false;
                for (int c = 0; c < fc->n_cond && !any; c++) any = pr_reward_fc_condition(ctx, p, &pr_fc_conds[fc->cond_start + c]);
                if (!any) continue;
            }
            if (fc->pre_form_key != key) continue;
            if (!(fc->flags & 4)) continue;   /* t.active */
            /* !findModifier(PokemonFormChangeItemModifier of this Pokemon with this item) */
            if (pr_reward_held(ctx, p, PR_ITEM_FORM_CHANGE_ITEM, fc->item) || pr_reward_held(ctx, p, PR_ITEM_RARE_FORM_CHANGE_ITEM, fc->item))
                continue;
            if (nm < 64) mine[nm++] = (uint16_t)(sl->start + i);
            else ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS;
        }
        if (m->species == SPECIES_ID_NECROZMA) {
            bool z = false, luna = false, solar = false;
            for (int i = 0; i < nm; i++) {
                const int it = pr_item_fcs[mine[i]].item;
                z |= it == FORM_CHANGE_ITEM_ULTRANECROZIUM_Z;
                luna |= it == FORM_CHANGE_ITEM_N_LUNARIZER;
                solar |= it == FORM_CHANGE_ITEM_N_SOLARIZER;
            }
            if (z && luna && solar) {
                int k = 0;
                for (int i = 0; i < nm; i++)
                    if (pr_item_fcs[mine[i]].item != FORM_CHANGE_ITEM_ULTRANECROZIUM_Z) mine[k++] = mine[i];
                nm = k;
            }
        }
        for (int i = 0; i < nm; i++) {
            if (list_has(trig, nt, mine[i])) continue;
            if (nt < 128) trig[nt++] = mine[i];
            else ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS;
        }
    }
    uint16_t pool[128];
    int n = 0;
    for (int i = 0; i < nt; i++) {
        const int it = pr_item_fcs[trig[i]].item;
        if ((it && it < 150) == rare) pool[n++] = (uint16_t)it;
    }
    if (!n) return GEN_NULL;
    return pool[seed_int(ctx, n)];
}

/* TERA_SHARD: null without the Tera Orb; the members' tera types (not Terapagos, Ogerpon or Shedinja, as species or
 * fusion); when all are the same type that one is excluded; randSeedInt(64) ? randSeedInt(18) : STELLAR, again while
 * it is the excluded one */
static bool has_species(const pr_reward_mon *m, int species) { return m->species == species || m->fusion_species == species; }
bool pr_reward_tera_excluded_species(const pr_reward_mon *m)
{
    return has_species(m, SPECIES_ID_TERAPAGOS) || has_species(m, SPECIES_ID_OGERPON) || has_species(m, SPECIES_ID_SHEDINJA);
}
static int gen_tera_shard(pr_reward_ctx *ctx)
{
    if (!pr_reward_has_item(ctx, PR_ITEM_TERA_ORB)) return GEN_NULL;
    int first = -100, n = 0;
    bool same = true;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (pr_reward_tera_excluded_species(m)) continue;
        if (!n) first = m->tera_type;
        else if (m->tera_type != first) same = false;
        n++;
    }
    const int excluded = n && same ? first : POKEMON_TYPE_UNKNOWN;
    int shard;
    do {
        shard = seed_int(ctx, 64) ? seed_int(ctx, 18) : POKEMON_TYPE_STELLAR;
    } while (shard == excluded);
    return shard;
}

/* generateType(party) for a generator item; the arg or GEN_NULL. -1 for a plain item (no roll). */
#ifdef PR_REWARD_DEBUG
#include <stdio.h>
extern int pr_reward_debug_case;
static int generate(pr_reward_ctx *ctx, int item);
int pr_reward_generate(pr_reward_ctx *ctx, int item)
{
    const int r = generate(ctx, item);
    if (pr_reward_debug_case && pr_items[item].gen) {
        if (r == GEN_NULL) fprintf(stderr, "G %s null\n", pr_item_keys[item]);
        else fprintf(stderr, "G %s %d\n", pr_item_keys[item], r);
    }
    return r;
}
static int generate(pr_reward_ctx *ctx, int item)
#else
int pr_reward_generate(pr_reward_ctx *ctx, int item)
#endif
{
    switch (pr_items[item].gen) {
    case PR_GEN_NONE: return -1;
    case PR_GEN_BERRY: {
        /* rand = randSeedInt(12): < 2 SITRUS, < 4 LUM, < 6 LEPPA, else berryTypes[randSeedInt(length - 3) + 2] */
        const int rand = seed_int(ctx, 12);
        if (rand < 2) return BERRY_TYPE_SITRUS;
        if (rand < 4) return BERRY_TYPE_LUM;
        if (rand < 6) return BERRY_TYPE_LEPPA;
        return seed_int(ctx, (BERRY_TYPE_LEPPA + 1) - 3) + 2;
    }
    case PR_GEN_BASE_STAT: return seed_int(ctx, STAT_SPD + 1);
    case PR_GEN_TEMP_STAT: return pr_rng_seed_int(&ctx->run->rng, STAT_ACC, STAT_ATK);   /* randSeedInt(Stat.ACC, Stat.ATK) */
    case PR_GEN_ATTACK_TYPE: return gen_attack_type(ctx);
    case PR_GEN_SPECIES_STAT: return gen_species_stat(ctx, false);
    case PR_GEN_RARE_SPECIES_STAT: return gen_species_stat(ctx, true);
    case PR_GEN_TM_COMMON: return gen_tm(ctx, MODIFIER_TIER_COMMON);
    case PR_GEN_TM_GREAT: return gen_tm(ctx, MODIFIER_TIER_GREAT);
    case PR_GEN_TM_ULTRA: return gen_tm(ctx, MODIFIER_TIER_ULTRA);
    case PR_GEN_EVOLUTION_ITEM: return gen_evolution_item(ctx, false);
    case PR_GEN_RARE_EVOLUTION_ITEM: return gen_evolution_item(ctx, true);
    case PR_GEN_FORM_CHANGE_ITEM: return gen_form_change_item(ctx, false);
    case PR_GEN_RARE_FORM_CHANGE_ITEM: return gen_form_change_item(ctx, true);
    case PR_GEN_MINT: return seed_int(ctx, PR_NATURE_COUNT);   /* randSeedItem(getEnumValues(Nature)) */
    case PR_GEN_TERA_SHARD: return gen_tera_shard(ctx);
    case PR_GEN_SHUCKLE_JUICE: return 10;
    case PR_GEN_BLACK_SLUDGE: return -1;
    }
    return GEN_NULL;
}

/* ---- the thresholds ---- */
/* regenerateModifierPoolThresholds(party, PLAYER, rerollCount): for each tier and entry in order,
 *   existingModifiers = findModifiers(m => m.type.id === id, true);
 *   itemModifierType = generator ? generateType(party) : type;           // the generators roll here
 *   weight = !existing.length || item is a PokemonHeldItemModifierType or FormChangeItemModifierType
 *            || existing.find(m => m.stackCount < m.getMaxStackCount(true)) ? weight(party, rerollCount) : 0;
 *   a non-zero weight adds to the tier's running total, recorded with the entry's index */
void pr_reward_regenerate_thresholds(pr_reward_ctx *ctx, int reroll_count, pr_reward_thresholds *th)
{
    memset(th, 0, sizeof *th);
    for (int t = 0; t < 6; t++) {
        const pr_pool_tier *pt = &pr_player_pool[t];
        int total = 0;
        for (int i = 0; i < pt->n; i++) {
            const pr_pool_entry_p *e = &pr_player_pool_entries[pt->start + i];
            bool existing = false, under_max = false;
            for (int k = 0; k < ctx->n_items; k++) {
                const pr_owned_item *o = &ctx->items[k];
                if (o->item != e->item) continue;
                existing = true;
                /* getMaxStackCount(true): a held item's is the most any member may hold (getMaxHeldItemCount; a vitamin,
                 * never null, never reaches this); 0 when its holder isn't in the party */
                const int max = o->owner >= 0 ? (o->owner < ctx->n_party ? pr_items[e->item].max_held : 0)
                                              : pr_items[e->item].max_stack_threshold;
                if (o->stack < max) under_max = true;
            }
            unsigned flags = pr_items[e->item].flags;
            if (pr_items[e->item].gen) {
                const int arg = pr_reward_generate(ctx, e->item);
                if (arg == GEN_NULL) flags = 0;   /* null: not a held or form change type */
            }
            int weight = 0;
            if (!existing || (flags & (PR_ITEMF_HELD | PR_ITEMF_FORM_CHANGE)) || under_max)
                weight = e->weight >= 0 ? e->weight : pr_reward_weight(ctx, e->item, t, reroll_count);
            if (!weight) continue;
            total += weight;
            if (th->n[t] == PR_REWARD_MAX_TIER_ENTRIES) { ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS; continue; }
            th->total[t][th->n[t]] = total;
            th->index[t][th->n[t]++] = i;
        }
    }
}

/* ---- one option ---- */
#define UNDEF (-100)
/* getNewModifierTypeOption(party, PLAYER, tier, upgradeCount, retryCount, allowLuckUpgrades); tier/upgrade UNDEF =
 * undefined. Returns false for null (a tier with no weight: upstream reads an undefined threshold). */
static bool new_option(pr_reward_ctx *ctx, const pr_reward_thresholds *th, int tier, int upgrade, int retry,
                       bool allow_luck, pr_reward_option *out)
{
    for (;;) {
        if (tier == UNDEF) {
            const int v = seed_int(ctx, 1024);
            if (upgrade == UNDEF || !upgrade) upgrade = 0;
            if (v && allow_luck) {
                /* upgradeOdds = floor(128 / ((luck + 4) / 4)); do { upgraded = randSeedInt(upgradeOdds) < 4 } while (upgraded) */
                const int odds = (int)floor(128 / ((ctx->luck + 4) / 4.0));
                while (seed_int(ctx, odds) < 4) upgrade++;
            }
            tier = v > 255 ? 0 : v > 60 ? 1 : v > 12 ? 2 : v ? 3 : 4;
            tier += upgrade;
            while (tier && (tier > 5 || pr_player_pool[tier].n <= 0)) {   /* !Object.hasOwn(pool, tier) || empty */
                tier--;
                if (upgrade) upgrade--;
            }
        } else if (upgrade == UNDEF) {
            upgrade = 0;
            if (tier < MODIFIER_TIER_MASTER && allow_luck) {
                const int odds = (int)floor(128 / ((ctx->luck + 4) / 4.0));
                while (tier + upgrade + 1 <= 5 && pr_player_pool[tier + upgrade + 1].n > 0) {
                    if (seed_int(ctx, odds) < 4) upgrade++;
                    else break;
                }
                tier += upgrade;
            }
        } else if (retry >= 100 && tier) {
            retry = 0;
            tier--;
        }
        /* totalWeight = the tier's last threshold; value = randSeedInt(totalWeight); the first threshold above it */
        if (!th->n[tier]) {
            pr_rng_frac(&ctx->run->rng);   /* randSeedInt(NaN) still rolls */
            ctx->run->unported |= PR_RUN_UNDEFINED_UPSTREAM;
            return false;
        }
        const int value = seed_int(ctx, th->total[tier][th->n[tier] - 1]);
        int index = -1;
        for (int i = 0; i < th->n[tier]; i++)
            if (value < th->total[tier][i]) { index = th->index[tier][i]; break; }
        if (index < 0) { ctx->run->unported |= PR_RUN_UNDEFINED_UPSTREAM; return false; }
        const pr_pool_entry_p *e = &pr_player_pool_entries[pr_player_pool[tier].start + index];
        int arg = -1;
        if (pr_items[e->item].gen) {
            arg = pr_reward_generate(ctx, e->item);
            /* null: getNewModifierTypeOption(party, poolType, tier, upgradeCount, ++retryCount) */
            if (arg == GEN_NULL) { retry++; allow_luck = true; continue; }
        }
        out->type = (pr_item_type){e->item, (int16_t)arg};
        out->tier = (int8_t)tier;
        out->upgrade_count = (int8_t)upgrade;
        out->cost = 0;
        return true;
    }
}

/* applyChallenges(WAVE_REWARD, candidate, validity): Hardcore turns down the revive group */
static bool wave_reward_valid(const pr_reward_ctx *ctx, const pr_reward_option *o)
{
    if (!(ctx->challenges & (1u << CHALLENGES_HARDCORE))) return true;
    const int g = pr_item_type_group(o->type);
    return g < 0 || !pr_reward_strings[g] || strcmp(pr_reward_strings[g], "revive") != 0;
}

/* getModifierTypeOptionWithRetry(existingOptions, retryCount, party, tier, allowLuckUpgrades):
 *   candidate = getNewModifierTypeOption(party, PLAYER, tier, undefined, 0, allowLuckUpgrades);
 *   while ((existing.length && ++r < retryCount && existing.some(o => same name || same group)) || !valid)
 *     candidate = getNewModifierTypeOption(party, PLAYER, candidate.type.tier ?? tier, candidate.upgradeCount, 0, allow);
 * (two undefined groups count as the same) */
static bool option_with_retry(pr_reward_ctx *ctx, const pr_reward_thresholds *th, const pr_reward_option *existing,
                              int n_existing, int retry_count, int tier, bool allow_luck, pr_reward_option *out)
{
    if (!new_option(ctx, th, tier, UNDEF, 0, allow_luck, out)) return false;
    bool valid = wave_reward_valid(ctx, out);
    int r = 0;
    for (;;) {
        bool dup = false;
        if (n_existing > 0 && ++r < retry_count) {
            const int name = pr_item_type_name(out->type), group = pr_item_type_group(out->type);
            for (int i = 0; i < n_existing && !dup; i++)
                dup = pr_item_type_name(existing[i].type) == name || pr_item_type_group(existing[i].type) == group;
        }
        if (!dup && valid) return true;
        if (!new_option(ctx, th, out->tier, out->upgrade_count, 0, allow_luck, out)) return false;
        valid = wave_reward_valid(ctx, out);
    }
}

/* withTierFromPool(PLAYER, party): the first tier whose entry for this item weighs more than 0 (weight functions with
 * rerollCount 0), else the first tier it is in, but only when that isn't COMMON (`if (defaultTier)`); -1 = undefined */
static int tier_from_pool(pr_reward_ctx *ctx, int item)
{
    int def = -1;
    for (int t = 0; t < 6; t++) {
        const pr_pool_tier *pt = &pr_player_pool[t];
        for (int i = 0; i < pt->n; i++) {
            const pr_pool_entry_p *e = &pr_player_pool_entries[pt->start + i];
            if (e->item != item) continue;
            const int w = e->weight >= 0 ? e->weight : pr_reward_weight(ctx, item, t, 0);
            if (w > 0) return t;
            if (def < 0) def = t;
        }
    }
    return def > 0 ? def : -1;
}

int pr_reward_get_player_options(pr_reward_ctx *ctx, const pr_reward_thresholds *th, int count, const int8_t *tiers,
                                 int n_tiers, const pr_reward_custom *custom, pr_reward_option *out)
{
    int n = 0;
    const int retry_count = count * 5 < 50 ? count * 5 : 50;
#define PUSH_CHECK()                                                                                                  \
    if (n >= PR_REWARD_MAX_OPTIONS) { ctx->run->unported |= PR_REWARD_UNPORTED_OPTIONS; return -1; }
    if (custom) {
        for (int i = 0; i < custom->n_options; i++) {
            PUSH_CHECK();
            out[n++] = custom->options[i];
        }
        for (int i = 0; i < custom->n_funcs; i++) {
            /* guaranteedMod = modifierTypeInitObj[id]().withIdFromFunc(...).withTierFromPool(PLAYER, party);
             * modType = generator ? generateType(party) : guaranteedMod; if (modType) push(new ModifierTypeOption(modType, 0)) */
            const int item = custom->funcs[i];
            if (item >= PR_ITEM_COUNT_) { ctx->run->unported |= PR_REWARD_UNPORTED_FUNC; return -1; }
            const int tier = tier_from_pool(ctx, item);
            const int arg = pr_reward_generate(ctx, item);
            if (arg == GEN_NULL) continue;
            PUSH_CHECK();
            out[n++] = (pr_reward_option){{(uint16_t)item, (int16_t)arg}, (int8_t)tier, 0, 0};
        }
        const bool allow = custom->allow_luck_upgrades;
        for (int i = 0; i < custom->n_tiers; i++) {
            PUSH_CHECK();
            if (!option_with_retry(ctx, th, out, n, retry_count, custom->tiers[i], allow, &out[n])) return -1;
            n++;
        }
        /* fillRemaining: getModifierTypeOptionWithRetry(options, retryCount, party, undefined) (luck upgrades allowed) */
        if (n < count && custom->fill_remaining)
            while (n < count) {
                PUSH_CHECK();
                if (!option_with_retry(ctx, th, out, n, retry_count, UNDEF, true, &out[n])) return -1;
                n++;
            }
    } else {
        for (int i = 0; i < count; i++) {
            PUSH_CHECK();
            const int tier = tiers && n_tiers > i && tiers[i] >= 0 ? tiers[i] : UNDEF;   /* -1: an undefined tier */
            if (!option_with_retry(ctx, th, out, n, retry_count, tier, true, &out[n])) return -1;
            n++;
        }
    }
#undef PUSH_CHECK
    return n;
}

/* getModifierCount(): 3, + each Golden Poke Ball's (ExtraModifierModifier) and the Silver Poke Ball's
 * (TempExtraModifierModifier) getStackCount(); custom settings: the guaranteed count, or the larger one with
 * fillRemaining */
int pr_reward_get_modifier_count(const pr_reward_ctx *ctx, const pr_reward_custom *custom)
{
    int count = 3;
    for (int i = 0; i < ctx->n_items; i++)
        if (ctx->items[i].item == PR_ITEM_GOLDEN_POKEBALL || ctx->items[i].item == PR_ITEM_SILVER_POKEBALL)
            count += ctx->items[i].stack;
    if (custom) {
        const int k = custom->n_tiers + custom->n_options + custom->n_funcs;
        count = custom->fill_remaining ? (count > k ? count : k) : k;
    }
    return count;
}

int pr_reward_select_start(pr_reward_ctx *ctx, const pr_reward_select *sel, pr_reward_thresholds *th,
                           pr_reward_option *out)
{
    if (!sel->reroll_count && !sel->is_copy) pr_run_reset_seed(ctx->run, ctx->wave);   /* updateSeed(): resetSeed() */
    if (!sel->is_copy) pr_reward_regenerate_thresholds(ctx, sel->reroll_count, th);
    const int count = pr_reward_get_modifier_count(ctx, sel->custom);
    return pr_reward_get_player_options(ctx, th, count, sel->locked_tiers, sel->n_locked_tiers, sel->custom, out);
}

/* ---- shop economy & Lock Capsule mechanics ---- */
double pr_reward_reroll_cost(const pr_reward_ctx *ctx, int reroll_count, bool lock_rarities,
                            const pr_reward_option *options, int n_options,
                            const pr_reward_custom *custom)
{
    if (custom && custom->reroll_multiplier < 0) {
        return -1.0;
    }

    double base_value = 250.0;
    if (lock_rarities) {
        static const double tier_values[5] = {50.0, 125.0, 300.0, 750.0, 2000.0};
        base_value = 0.0;
        if (options && n_options > 0) {
            for (int i = 0; i < n_options; i++) {
                int t = options[i].tier;
                if (t < 0 || t > 4) t = 0;
                base_value += tier_values[t];
            }
        }
    }

    double multiplier = 1.0;
    if (custom && !isnan(custom->reroll_multiplier)) {
        multiplier = custom->reroll_multiplier;
    }

    int wave = ctx ? ctx->wave : 1;
    if (wave < 1) wave = 1;
    double wave_factor = ceil(wave / 10.0);

    double base_multiplier = wave_factor * base_value * pow(2.0, reroll_count) * multiplier;
    if (base_multiplier > 9007199254740991.0) {
        base_multiplier = 9007199254740991.0;
    }

    double bs_mult = ctx ? pr_reward_black_sludge_shop_cost_multiplier(ctx) : 1.0;
    return floor(base_multiplier * bs_mult);
}

int pr_reward_get_locked_tiers(const pr_reward_option *options, int n_options, int8_t *out_tiers)
{
    if (!options || n_options <= 0 || !out_tiers) return 0;
    for (int i = 0; i < n_options; i++) {
        out_tiers[i] = options[i].tier;
    }
    return n_options;
}

bool pr_reward_try_deduct_money(double *money, double cost)
{
    if (!money || cost < 0.0 || *money < cost) return false;
    *money -= cost;
    return true;
}

int pr_reward_reroll(pr_reward_ctx *ctx, double *money, bool lock_rarities,
                     const pr_reward_option *prev_options, int n_prev_options,
                     pr_reward_select *sel, pr_reward_thresholds *th,
                     pr_reward_option *out_options)
{
    if (!ctx || !money || !sel || !th || !out_options) return -1;

    const pr_reward_custom *custom = sel->custom;
    const double cost = pr_reward_reroll_cost(ctx, sel->reroll_count, lock_rarities,
                                              prev_options, n_prev_options, custom);
    if (cost < 0.0 || !pr_reward_try_deduct_money(money, cost)) {
        return -1;
    }

    int8_t locked_tiers[PR_REWARD_MAX_OPTIONS];
    int n_locked = 0;
    if (lock_rarities && prev_options && n_prev_options > 0) {
        n_locked = pr_reward_get_locked_tiers(prev_options, n_prev_options, locked_tiers);
    }

    sel->reroll_count++;
    sel->is_copy = false;
    if (lock_rarities && n_locked > 0) {
        sel->locked_tiers = locked_tiers;
        sel->n_locked_tiers = n_locked;
    } else {
        sel->locked_tiers = NULL;
        sel->n_locked_tiers = 0;
    }

    int res = pr_reward_select_start(ctx, sel, th, out_options);
    sel->locked_tiers = NULL;
    sel->n_locked_tiers = 0;
    return res;
}
