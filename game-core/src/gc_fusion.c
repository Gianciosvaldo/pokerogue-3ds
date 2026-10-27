/*
 * DNA Splicers taken from the reward screen: FusePokemonModifierType (src/modifier/modifier-type.ts), the party screen's SPLICE mode
 * that SelectModifierPhase.openFusionMenu opens, and FusePokemonModifier.apply -> PlayerPokemon.fuse (src/field/pokemon.ts).
 *
 * reward-core decides which pairs may fuse and what the fusion half is (tested against upstream in reward-core/tests). What it can't see is
 * the run around it, and that is this file: the held items of both members and the stats they change (reward-core's Pokemon carries no
 * items), the hp and the status chosen by the coin flip, the second member leaving the party, and the LearnMovePhase upstream queues for
 * each of its moves. Tested against upstream's fuse() as a whole by tests/fusion.producer.test.ts (tools/fusiondiff.sh).
 *
 * PlayerPokemon.fuse, in order:
 *   1. the fusion half is set (species, form, ability index, shiny, variant, gender, luck, custom data, pauseEvolutions)
 *   2. newHpPercent = (the second's hp / its max + this hp / this old max) / 2, the maxima as they are BEFORE the fusion
 *   3. calculateStats(): the fusion's base stats (the held items of this Pokemon only; its hp moves by the change of the maximum)
 *   4. hp = round(old max * newHpPercent); a living result takes the smaller of that and the old max and a status chosen by
 *      getRandomStatus (an unseeded coin flip when both have one), a fainted one with a living second member takes hp >= 1 and the
 *      second member's status
 *   5. every held item of the second member moves to this one (tryTransferHeldItemModifier, ignoreUpdate)
 *   6. updateModifiers(true, true): items of Pokemon that are gone are dropped, every member's stats are calculated again (this is where
 *      a vitamin that came with the second member raises the maximum, and the hp with it)
 *   7. removePartyMemberModifiers(second): what could not move (a stack at its maximum) goes with it; the second member leaves the party
 *   8. one LearnMovePhase per move of the second member, for this Pokemon's place in the party after the removal
 */
#include <math.h>
#include <string.h>

#include "internal.h"

static bool has_status(int status) { return status != STATUS_EFFECT_NONE && status != STATUS_EFFECT_FAINT; }

/* DNA Splicers through reward-core: the pair at reward-core's party positions k1 (stays) and k2 (goes) in a fresh context. What it
 * decides is whether the pair may fuse (the fusion filter, the challenge) and the fusion half the first one takes; its hp, status and
 * stats are not used: it rounds the average hp with floor(x + 0.5), which is not Math.round at 0.49999999999999994, and it has no held
 * items to calculate the stats with */
static pr_reward_ctx *fuse_in_core(gc_run *run, int k1, int k2)
{
    pr_reward_ctx *c = gc_reward_ctx_base(run);
    c->challenges = run->run.has_challenges ? run->run.challenge_flags : 0;
    c->fusion_status_choice_available = true;   /* the coin is not asked of reward-core: the status is set below */
    c->fusion_status_choice = 1;
    const char *msg = NULL;
    /* arg -1: the second member is move_idx (an arg of 0 would be read as a party index) */
    return pr_reward_apply(c, PR_ITEM_DNA_SPLICERS, -1, k1, k2, &msg) ? c : NULL;
}

static void take_status(pr_pokemon *to, const pr_pokemon *from)
{
    to->status = from->status;
    to->status_toxic_turns = from->status_toxic_turns;
    to->status_sleep_turns = from->status_sleep_turns;
    to->status_freeze_turns = from->status_freeze_turns;
}

int gc_fusion_scene_apply(gc_run *run, const gc_offer *o, int primary, int donor, const char **why)
{
    (void)o;
    int k1, k2;
    if (primary < 0 || primary >= run->n_party || donor < 0 || donor >= run->n_party || primary == donor
        || (k1 = gc_battle_slot_of(run, primary)) < 0 || (k2 = gc_battle_slot_of(run, donor)) < 0) {
        /* openFusionMenu: a pick of the same member twice, or leaving the screen, is resetModifierSelect: the reward is still there */
        *why = "pick two Pokemon";
        return GC_APPLY_REFUSED;
    }
    gc_mon *a = &run->party[primary];
    const gc_mon *b = &run->party[donor];

    /* the pair as reward-core accepts it, and the fusion half it makes */
    pr_reward_ctx *c = fuse_in_core(run, k1, k2);
    if (!c) {
        *why = "partyUiHandler:anyEffect";
        return GC_APPLY_REFUSED;
    }
    const pr_reward_mon *r = &c->party[k1 > k2 ? k1 - 1 : k1];   /* the second member is gone from reward-core's party */

    /* step 1 */
    pr_pokemon *p = &a->b;
    const pr_pokemon *q = &b->b;
    const uint32_t donor_id = q->id;
    uint16_t donor_moves[4];
    int n_donor_moves = 0;
    for (int i = 0; i < q->n_moves && i < 4; i++) if (q->moves[i]) donor_moves[n_donor_moves++] = q->moves[i];

    p->fusion_species = (uint16_t)r->fusion_species;
    p->fusion_form = r->fusion_form;
    p->fusion_gender = r->fusion_gender;
    p->fusion_gender_known = true;
    p->fusion_ability_index = r->fusion_ability_index;
    p->fusion_custom_ability = q->custom_ability;   /* fusionCustomPokemonData = pokemon.customPokemonData */
    a->fusion_shiny = r->fusion_shiny;
    a->fusion_variant = r->fusion_variant;
    a->fusion_luck = r->fusion_luck;
    a->pause_evolutions = r->pause_evolutions;
    /* getAbility's fusion branch: the fusion half's ability at the donor's ability index */
    {
        const pr_form *ff = gc_form(p->fusion_species, p->fusion_form);
        const int fi = p->fusion_ability_index;
        p->base_ability = fi == 0 ? ff->ability1 : fi == 1 ? ff->ability2 : ff->ability_hidden;
        if (p->ability_override <= 0) p->ability = p->base_ability;
        pr_refresh_ability(p);
    }

    /* step 2: the maxima as they are before the fusion (Math.round: js_round) */
    const int old_max = a->b.stats[STAT_HP];
    const double pct = ((double)b->b.hp / b->b.stats[STAT_HP] + (double)a->b.hp / old_max) / 2.0;
    const int new_hp = (int)js_round((double)old_max * pct);

    /* step 3 */
    gc_mon_recalc(run, a);

    /* step 4: getRandomStatus draws only when both have a status, and fuse() only reaches it with hp left. The flip is unseeded upstream,
     * so the stand-ins' own generator (it can't move upstream's seeded rolls); 0 = the second member's status, 1 = this one's */
    p->hp = new_hp;
    if (p->hp > 0) {
        if (p->hp > old_max) p->hp = old_max;
        if (!has_status(p->status)) take_status(p, q);
        else if (has_status(q->status) && gc_stub_rand(run, 2) == 0) take_status(p, q);
    } else if (q->hp > 0) {
        p->hp = 1;
        take_status(p, q);
    }

    /* steps 5 and 6 */
    gc_items_fuse_transfer(run, donor_id, primary);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);

    /* step 7: removePartyMemberModifiers, updateModifiers, then the splice. The battle's pointers into the storage move with it */
    gc_items_drop_held_of(run, donor_id);
    gc_me_update_modifiers(run);
    int field_at[2] = {-1, -1};
    for (int f = 0; f < 2; f++)
        for (int i = 0; i < run->n_party; i++) if (run->bt.field[f] == &run->party[i].b) field_at[f] = i;
    gc_party_remove(run, donor);
    const int now = primary > donor ? primary - 1 : primary;
    for (int f = 0; f < 2; f++) {
        if (field_at[f] < 0) continue;
        run->bt.field[f] = field_at[f] == donor ? NULL : &run->party[field_at[f] > donor ? field_at[f] - 1 : field_at[f]].b;
    }
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
    gc_items_to_battle(run);

    gc_event *ev = gc_ev_new(run, GC_EV_HP);
    if (ev) { ev->a = 0; ev->b = now; ev->c = run->party[now].b.hp; }

    /* step 8: unshiftNew in the order of the moveset, so the queue runs them as the level-up moves are run */
    for (int i = 0; i < n_donor_moves; i++) gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, now, donor_moves[i], 0));
    return GC_APPLY_DONE;
}
