/* PokemonNatureChangeModifierType selection + PokemonNatureChangeModifier.apply,
 * pinned upstream 8555c08. Unlock policy belongs to the caller's dex, not rewards. */
#include "pr/reward.h"
#include "pr/battle.h"
#include <string.h>

bool pr_reward_apply_mint(pr_reward_ctx *ctx, int nature, int party_idx)
{
    if (!ctx || ctx->n_party < 1 || ctx->n_party > PR_REWARD_MAX_PARTY
        || party_idx < 0 || party_idx >= ctx->n_party
        || nature < 0 || nature >= PR_NATURE_COUNT
        || ctx->n_items < 0 || ctx->n_items > PR_REWARD_MAX_OWNED
        || ctx->n_nature_unlock_events >= PR_REWARD_MAX_NATURE_EVENTS
        || (ctx->challenges & (1u << CHALLENGES_FLIP_STAT))) return false;
    pr_reward_mon *m = &ctx->party[party_idx];
    if (!m->has_explicit_nature || m->base_nature >= PR_NATURE_COUNT
        || m->custom_nature < -1 || m->custom_nature >= PR_NATURE_COUNT
        || m->nature != (m->custom_nature < 0 ? m->base_nature : m->custom_nature)
        || m->nature == nature || !m->level || m->level > UINT8_MAX || m->max_hp < 1 || m->hp < 0
        || m->fusion_species < -1
        || m->hp > m->max_hp || m->stats[STAT_HP] != m->max_hp
        || m->ability >= PR_ABILITY_COUNT || m->passive >= PR_ABILITY_COUNT
        || (m->fusion_species < 0 && ctx->run && ctx->run->mode == GAME_MODES_SPLICED_ENDLESS)) return false;
    for (int s = 0; s < 6; s++) if (m->ivs[s] > 31) return false;
    /* These inventories alter calculateStats, not just in-battle effective stats.
     * Their translation belongs to the later stat-item prerequisite. Reject even
     * inactive entries: upstream's applyModifiers does not generally filter active. */
    for (int j = 0; j < ctx->n_items; j++) {
        const pr_owned_item *it = &ctx->items[j];
        if (it->item >= PR_ITEM_COUNT_ || it->owner < -1 || it->owner >= ctx->n_party || it->stack < 0) return false;
        if (it->stack <= 0 || (it->owner != party_idx && it->owner != -1)) continue;
        if (it->item == PR_ITEM_BASE_STAT_BOOSTER || it->item == PR_ITEM_SOUL_DEW
            || it->item == PR_ITEM_MYSTERY_ENCOUNTER_SHUCKLE_JUICE
            || it->item == PR_ITEM_MYSTERY_ENCOUNTER_OLD_GATEAU
            || it->item == PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE) return false;
    }
    pr_pokemon p = {0}; pr_battle bt = {0};
    p.species = m->species; p.form = m->form;
    p.fusion_species = m->fusion_species < 0 ? 0 : m->fusion_species;
    p.fusion_form = m->fusion_form;
    const pr_species *s = pr_pokemon_species(&p), *f = pr_pokemon_fusion_species(&p);
    if (!s || m->form < 0 || m->form >= s->nforms
        || (m->fusion_species >= 0 && (!f || m->fusion_form < 0 || m->fusion_form >= f->nforms))) return false;
    p.level = m->level; p.nature = nature;
    p.base_ability = m->ability; p.passive = m->passive;
    p.is_player = true; p.id = m->id; p.hp = m->hp;
    memcpy(p.ivs, m->ivs, sizeof p.ivs);
    memcpy(p.stats, m->stats, sizeof p.stats);
    pr_recalculate_stats(&bt, &p);
    m->custom_nature = nature; m->nature = nature; /* base_nature is deliberately unchanged */
    memcpy(m->stats, p.stats, sizeof m->stats);
    m->max_hp = p.stats[STAT_HP]; m->hp = p.hp;
    int index = ctx->n_nature_unlock_events++;
    ctx->nature_unlock_events[index].species = m->species; /* primary, never the fusion */
    ctx->nature_unlock_events[index].nature = nature;
    return true;
}
