/* Pinned 8555c08 BaseStatBoosterModifierType, PersistentModifier.add and
 * BattleScene.updateModifiers. Caller owns UI, affordability and persistence. */
#include "pr/reward.h"
#include "pr/battle.h"
#include <math.h>
#include <string.h>

static bool stat_mon(const pr_reward_mon *m, pr_pokemon *p)
{
    if (!m->level || m->level > UINT8_MAX || m->nature >= PR_NATURE_COUNT
        || m->fusion_species < -1 || m->ability >= PR_ABILITY_COUNT || m->passive >= PR_ABILITY_COUNT
        || m->max_hp < 1 || m->hp < 0 || m->hp > m->max_hp || m->stats[STAT_HP] != m->max_hp) return false;
    if (m->has_explicit_nature && (m->base_nature >= PR_NATURE_COUNT
        || m->custom_nature < -1 || m->custom_nature >= PR_NATURE_COUNT
        || m->nature != (m->custom_nature < 0 ? m->base_nature : m->custom_nature))) return false;
    for (int j = 0; j < 6; j++) if (m->ivs[j] > 31) return false;
    p->species = m->species; p->form = m->form;
    p->fusion_species = m->fusion_species < 0 ? 0 : m->fusion_species; p->fusion_form = m->fusion_form;
    const pr_species *s = pr_pokemon_species(p), *f = pr_pokemon_fusion_species(p);
    if (!s || m->form < 0 || m->form >= s->nforms
        || (m->fusion_species >= 0 && (!f || m->fusion_form < 0 || m->fusion_form >= f->nforms))) return false;
    p->level = m->level; p->nature = m->nature; p->base_ability = m->ability; p->passive = m->passive;
    p->is_player = true; p->id = m->id; p->hp = m->hp;
    memcpy(p->ivs, m->ivs, sizeof p->ivs); memcpy(p->stats, m->stats, sizeof p->stats);
    return true;
}

enum pr_reward_vitamin_status pr_reward_apply_vitamin(pr_reward_ctx *ctx, int stat,
    int party_idx, double cost, pr_reward_vitamin_result *result)
{
    if (result) memset(result, 0, sizeof *result);
    if (!ctx || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || party_idx < 0 || party_idx >= ctx->n_party || stat < 0 || stat >= 6
        || !isfinite(cost) || (cost != -1 && cost < 0)
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED
        || (ctx->challenges & (1u << CHALLENGES_FLIP_STAT))) return PR_REWARD_VITAMIN_UNSUPPORTED;
    pr_pokemon party[PR_REWARD_MAX_PARTY] = {{0}}; pr_battle bt = {0};
    for (int j = 0; j < ctx->n_party; j++) {
        if (!stat_mon(&ctx->party[j], &party[j])
            || (ctx->party[j].fusion_species < 0 && ctx->run && ctx->run->mode == GAME_MODES_SPLICED_ENDLESS))
            return PR_REWARD_VITAMIN_UNSUPPORTED;
        for (int k = 0; k < j; k++) if (party[k].id == party[j].id) return PR_REWARD_VITAMIN_UNSUPPORTED;
    }
    int matching = -1;
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_ || it->owner < -1 || it->owner >= ctx->n_party || it->stack <= 0
            /* Generated held types need not carry bit PERSISTENT: dummy construction
             * has no holder. HELD/FORM_CHANGE still identify persistent subclasses. */
            || !(pr_items[it->item].flags & (PR_ITEMF_PERSISTENT | PR_ITEMF_HELD | PR_ITEMF_FORM_CHANGE))
            || it->item == PR_ITEM_EVOLUTION_TRACKER_GIMMIGHOUL
            || (it->owner < 0 && (pr_items[it->item].flags & (PR_ITEMF_HELD | PR_ITEMF_FORM_CHANGE))))
            return PR_REWARD_VITAMIN_UNSUPPORTED;
        if (!(pr_items[it->item].flags & (PR_ITEMF_HELD | PR_ITEMF_FORM_CHANGE)) && it->owner != -1)
            return PR_REWARD_VITAMIN_UNSUPPORTED; /* globals have no Pokemon holder */
        if (it->item == PR_ITEM_SOUL_DEW || it->item == PR_ITEM_MYSTERY_ENCOUNTER_SHUCKLE_JUICE
            || it->item == PR_ITEM_MYSTERY_ENCOUNTER_OLD_GATEAU || it->item == PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE)
            return PR_REWARD_VITAMIN_UNSUPPORTED;
        if (it->item != PR_ITEM_BASE_STAT_BOOSTER) continue;
        if (it->arg < 0 || it->arg >= 6 || it->owner < 0
            || it->stack > ctx->party[it->owner].ivs[it->arg] || bt.n_mods[0] >= PR_MAX_MODIFIERS)
            return PR_REWARD_VITAMIN_UNSUPPORTED;
        for (int k = 0; k < j; k++) if (ctx->items[k].item == it->item && ctx->items[k].arg == it->arg
            && ctx->items[k].owner == it->owner) return PR_REWARD_VITAMIN_UNSUPPORTED;
        pr_modifier *mod = &bt.mods[0][bt.n_mods[0]++];
        mod->cls = PR_MOD_BASE_STAT; mod->pokemon_id = party[it->owner].id;
        mod->stat = it->arg; mod->stack_count = it->stack;
        if (it->owner == party_idx && it->arg == stat) matching = j;
    }
    int cap = ctx->party[party_idx].ivs[stat];
    if (!cap) return PR_REWARD_VITAMIN_INOPERABLE;
    if (matching >= 0 && ctx->items[matching].stack == cap) return PR_REWARD_VITAMIN_MAX_STACK;
    if (matching < 0 && (ctx->n_items >= PR_REWARD_MAX_OWNED || bt.n_mods[0] >= PR_MAX_MODIFIERS))
        return PR_REWARD_VITAMIN_UNSUPPORTED;
    /* All refusals precede mutation. One persistent vitamin is added or stacked. */
    if (matching >= 0) {
        ctx->items[matching].stack++;
        for (int j = 0; j < bt.n_mods[0]; j++) if (bt.mods[0][j].pokemon_id == party[party_idx].id
            && bt.mods[0][j].stat == stat) { bt.mods[0][j].stack_count++; break; }
    } else {
        ctx->items[ctx->n_items++] = (pr_owned_item){.item = PR_ITEM_BASE_STAT_BOOSTER,
            .arg = stat, .owner = party_idx, .stack = 1, .active = true};
        pr_modifier *mod = &bt.mods[0][bt.n_mods[0]++];
        mod->cls = PR_MOD_BASE_STAT; mod->pokemon_id = party[party_idx].id; mod->stat = stat; mod->stack_count = 1;
    }
    /* updateModifiers recalculates every party member, not only the new holder. */
    for (int j = 0; j < ctx->n_party; j++) {
        pr_recalculate_stats(&bt, &party[j]);
        ctx->party[j].hp = party[j].hp; ctx->party[j].max_hp = party[j].stats[STAT_HP];
        memcpy(ctx->party[j].stats, party[j].stats, sizeof ctx->party[j].stats);
    }
    if (result) {
        result->added = true;
        result->add_modifier_result = false; /* upstream persistent branch never sets success */
        result->purchase_error = cost != -1; result->reward_finished = cost == -1;
    }
    return PR_REWARD_VITAMIN_APPLIED;
}
