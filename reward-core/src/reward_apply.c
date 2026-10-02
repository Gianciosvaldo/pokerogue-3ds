#include "pr/reward.h"
#include "reward_internal.h"
#include "pr_data.h"
#include <math.h>
#include <stddef.h>
#include <stdbool.h>

static const char NO_EFFECT_MSG[] = "It won’t have any effect.";

static double get_healing_booster_multiplier(const pr_reward_ctx *ctx) {
    double mult = 1.0;
    for (int i = 0; i < ctx->n_items; i++) {
        if (ctx->items[i].item == PR_ITEM_HEALING_CHARM && ctx->items[i].stack > 0) {
            mult += 0.10 * ctx->items[i].stack;
        }
    }
    return mult;
}

bool pr_reward_apply(pr_reward_ctx *ctx, uint16_t item, int16_t arg, int party_idx, int move_idx, const char **msg) {
    if (!ctx) {
        if (msg) *msg = NO_EFFECT_MSG;
        return false;
    }

    const bool hardcore = (ctx->challenges & (1u << CHALLENGES_HARDCORE)) != 0;

    switch (item) {
        case PR_ITEM_POTION:
        case PR_ITEM_SUPER_POTION:
        case PR_ITEM_HYPER_POTION:
        case PR_ITEM_MAX_POTION:
        case PR_ITEM_FULL_RESTORE: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            if (p->hp <= 0) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            int restore_pts = 0;
            int restore_pct = 0;
            bool heal_status = false;

            if (item == PR_ITEM_POTION) { restore_pts = 20; restore_pct = 10; }
            else if (item == PR_ITEM_SUPER_POTION) { restore_pts = 50; restore_pct = 25; }
            else if (item == PR_ITEM_HYPER_POTION) { restore_pts = 200; restore_pct = 50; }
            else if (item == PR_ITEM_MAX_POTION) { restore_pts = 0; restore_pct = 100; }
            else if (item == PR_ITEM_FULL_RESTORE) { restore_pts = 0; restore_pct = 100; heal_status = true; }

            if (p->hp >= p->max_hp) {
                if (!heal_status || p->status == 0) {
                    if (msg) *msg = NO_EFFECT_MSG;
                    return false;
                }
            }

            double mult = get_healing_booster_multiplier(ctx);
            int restore_boosted = (int)floor((double)restore_pts * mult);
            int percent_pts = (int)floor((double)restore_pct * 0.01 * (double)p->max_hp);
            int total_heal = (restore_boosted > percent_pts) ? restore_boosted : percent_pts;
            if (total_heal < 1) total_heal = 1;

            p->hp += total_heal;
            if (p->hp > p->max_hp) p->hp = p->max_hp;

            if (heal_status) {
                p->status = 0;
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_REVIVE:
        case PR_ITEM_MAX_REVIVE: {
            if (party_idx < 0 || party_idx >= ctx->n_party || hardcore) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            if (p->hp > 0) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            int restore_pct = (item == PR_ITEM_REVIVE) ? 50 : 100;
            p->status = 0;

            int percent_pts = (int)floor((double)restore_pct * 0.01 * (double)p->max_hp);
            if (percent_pts < 1) percent_pts = 1;
            p->hp = percent_pts;
            if (p->hp > p->max_hp) p->hp = p->max_hp;

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_SACRED_ASH: {
            bool any_fainted = false;
            for (int i = 0; i < ctx->n_party; i++) {
                if (ctx->party[i].hp <= 0) {
                    any_fainted = true;
                    break;
                }
            }
            if (!any_fainted) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            for (int i = 0; i < ctx->n_party; i++) {
                if (ctx->party[i].hp <= 0) {
                    ctx->party[i].status = 0;
                    ctx->party[i].hp = ctx->party[i].max_hp;
                }
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_ETHER:
        case PR_ITEM_MAX_ETHER: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            if (move_idx < 0 || move_idx >= p->n_moves) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            if (p->moves[move_idx].pp_used == 0) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            if (item == PR_ITEM_ETHER) {
                if (p->moves[move_idx].pp_used > 10) {
                    p->moves[move_idx].pp_used -= 10;
                } else {
                    p->moves[move_idx].pp_used = 0;
                }
            } else {
                p->moves[move_idx].pp_used = 0;
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_ELIXIR:
        case PR_ITEM_MAX_ELIXIR: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            bool any_used = false;
            for (int i = 0; i < p->n_moves; i++) {
                if (p->moves[i].pp_used > 0) {
                    any_used = true;
                    break;
                }
            }
            if (!any_used) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            for (int i = 0; i < p->n_moves; i++) {
                if (item == PR_ITEM_ELIXIR) {
                    if (p->moves[i].pp_used > 10) {
                        p->moves[i].pp_used -= 10;
                    } else {
                        p->moves[i].pp_used = 0;
                    }
                } else {
                    p->moves[i].pp_used = 0;
                }
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_PP_UP:
        case PR_ITEM_PP_MAX: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            if (move_idx < 0 || move_idx >= p->n_moves) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            uint16_t move_id = p->moves[move_idx].move;
            int base_pp = pr_moves[move_id].pp;

            if (base_pp < 5 || p->moves[move_idx].pp_up >= 3) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            int up = (item == PR_ITEM_PP_UP) ? 1 : 3;
            int new_pp_up = p->moves[move_idx].pp_up + up;
            if (new_pp_up > 3) new_pp_up = 3;

            p->moves[move_idx].pp_up = (uint8_t)new_pp_up;
            p->moves[move_idx].max_pp = (uint8_t)floor((double)base_pp * (1.0 + 0.2 * (double)new_pp_up));

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_EVOLUTION_ITEM:
        case PR_ITEM_RARE_EVOLUTION_ITEM: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *m = &ctx->party[party_idx];
            bool can_evolve = false;

            /* Check species evolutions */
            if (m->species >= 0) {
                const pr_reward_species *s = pr_reward_species_find(m->species);
                if (s && (s->flags & 1) && !pr_reward_form_is_max(m->species, m->form)) {
                    if (!m->pause_evolutions || pr_reward_pause_exempt(m->species)) {
                        const pr_species_slice *sl = pr_reward_slice_find(pr_item_evo_species, pr_item_evo_species_count, m->species);
                        if (sl) {
                            const int key = pr_reward_form_key(m->species, m->form);
                            for (int i = 0; i < sl->n; i++) {
                                const pr_item_evo *e = &pr_item_evos[sl->start + i];
                                if (e->item == arg && m->level >= e->level) {
                                    if (e->pre_form_key < 0 || e->pre_form_key == key) {
                                        if (pr_reward_evo_conditions(ctx, party_idx, e)) {
                                            can_evolve = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /* Check fusion species evolutions if not already can_evolve */
            if (!can_evolve && m->fusion_species >= 0) {
                const pr_reward_species *s = pr_reward_species_find(m->fusion_species);
                if (s && (s->flags & 1) && !pr_reward_form_is_max(m->fusion_species, m->fusion_form)) {
                    if (!m->pause_evolutions || pr_reward_pause_exempt(m->fusion_species)) {
                        const pr_species_slice *sl = pr_reward_slice_find(pr_item_evo_species, pr_item_evo_species_count, m->fusion_species);
                        if (sl) {
                            const int key = pr_reward_form_key(m->fusion_species, m->fusion_form);
                            for (int i = 0; i < sl->n; i++) {
                                const pr_item_evo *e = &pr_item_evos[sl->start + i];
                                if (e->item == arg && m->level >= e->level) {
                                    if (e->pre_form_key < 0 || e->pre_form_key == key) {
                                        if (pr_reward_evo_conditions(ctx, party_idx, e)) {
                                            can_evolve = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!can_evolve) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_FORM_CHANGE_ITEM:
        case PR_ITEM_RARE_FORM_CHANGE_ITEM: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *m = &ctx->party[party_idx];
            const pr_reward_species *s = pr_reward_species_find(m->species);
            if (!s || !(s->flags & 2)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            const pr_species_slice *sl = pr_reward_slice_find(pr_item_fc_species, pr_item_fc_species_count, m->species);
            if (!sl) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            const int key = pr_reward_form_key(m->species, m->form);

            bool can_change = false;
            for (int i = 0; i < sl->n; i++) {
                const pr_item_fc *fc = &pr_item_fcs[sl->start + i];
                if (fc->item != arg) continue;
                if (fc->pre_form_key != key) continue;
                if (!(fc->flags & 4)) continue; /* active */

                /* Check if already held */
                if (pr_reward_held(ctx, party_idx, PR_ITEM_FORM_CHANGE_ITEM, arg) ||
                    pr_reward_held(ctx, party_idx, PR_ITEM_RARE_FORM_CHANGE_ITEM, arg)) {
                    continue;
                }

                bool conds_met = true;
                for (int c = 0; c < fc->n_cond; c++) {
                    if (!pr_reward_fc_condition(ctx, party_idx, &pr_fc_conds[fc->cond_start + c])) {
                        conds_met = false;
                        break;
                    }
                }
                if (!conds_met) continue;

                can_change = true;
                break;
            }

            if (!can_change) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            /* Add form change item to owned items */
            if (ctx->n_items < PR_REWARD_MAX_OWNED) {
                ctx->items[ctx->n_items].item = item;
                ctx->items[ctx->n_items].arg = arg;
                ctx->items[ctx->n_items].stack = 1;
                ctx->items[ctx->n_items].owner = (int8_t)party_idx;
                ctx->items[ctx->n_items].active = true;
                ctx->n_items++;
            }

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_MEGA_BRACELET:
        case PR_ITEM_DYNAMAX_BAND: {
            if (pr_reward_has_item(ctx, item)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            if (ctx->n_items >= PR_REWARD_MAX_OWNED) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            ctx->items[ctx->n_items].item = item;
            ctx->items[ctx->n_items].arg = 0;
            ctx->items[ctx->n_items].stack = 1;
            ctx->items[ctx->n_items].owner = -1;
            ctx->items[ctx->n_items].active = true;
            ctx->items[ctx->n_items].battle_count = 0;
            ctx->n_items++;

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_DNA_SPLICERS: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            int sec_idx = (arg >= 0 && arg < ctx->n_party && arg != party_idx) ? arg : move_idx;
            if (sec_idx < 0 || sec_idx >= ctx->n_party || sec_idx == party_idx) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            pr_reward_mon *p1 = &ctx->party[party_idx];
            pr_reward_mon *p2 = &ctx->party[sec_idx];

            if (p1->fusion_species >= 0 || p2->fusion_species >= 0) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            if (!p1->allowed_in_challenge || !p2->allowed_in_challenge) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            /* Perform fusion */
            p1->fusion_species = p2->species;
            p1->fusion_form = p2->form;
            p1->fusion_gender = p2->gender;
            if (p2->pause_evolutions) p1->pause_evolutions = true;

            int sp1_id = p1->species > 1025 ? (p1->species % 1000) : p1->species;
            int sp2_id = p2->species > 1025 ? (p2->species % 1000) : p2->species;
            int hp1_base = 0, hp2_base = 0;
            if (sp1_id >= 1 && sp1_id <= PR_SPECIES_COUNT) {
                const pr_species *s = &pr_species_table[sp1_id - 1];
                uint16_t fidx = s->form0 + (p1->form < s->nforms ? p1->form : 0);
                if (fidx < PR_FORM_COUNT) hp1_base = pr_forms[fidx].base_stats[0];
            }
            if (sp2_id >= 1 && sp2_id <= PR_SPECIES_COUNT) {
                const pr_species *s = &pr_species_table[sp2_id - 1];
                uint16_t fidx = s->form0 + (p2->form < s->nforms ? p2->form : 0);
                if (fidx < PR_FORM_COUNT) hp2_base = pr_forms[fidx].base_stats[0];
            }

            double p1_hp_pct = (p1->max_hp > 0) ? ((double)p1->hp / (double)p1->max_hp) : 0.0;
            double p2_hp_pct = (p2->max_hp > 0) ? ((double)p2->hp / (double)p2->max_hp) : 0.0;
            double new_hp_pct = (p1_hp_pct + p2_hp_pct) / 2.0;

            if (hp1_base > 0 && hp2_base > 0) {
                int fused_base = (hp1_base + hp2_base) / 2;
                int delta_hp = ((2 * fused_base - 2 * hp1_base) * (int)p1->level) / 100;
                p1->max_hp += delta_hp;
                if (p1->max_hp < 1) p1->max_hp = 1;
            }

            bool p1_was_fainted = (p1->hp <= 0);
            bool p2_was_fainted = (p2->hp <= 0);

            int new_hp = (int)round((double)p1->max_hp * new_hp_pct);

            if (!p1_was_fainted) {
                p1->hp = new_hp;
                if (p1->hp > p1->max_hp) p1->hp = p1->max_hp;
                if (p1->status == 0 && p2->status != 0) {
                    p1->status = p2->status;
                }
            } else if (!p2_was_fainted) {
                p1->hp = (new_hp < 1) ? 1 : (new_hp > p1->max_hp ? p1->max_hp : new_hp);
                p1->status = p2->status;
            } else {
                p1->hp = 0;
            }

            /* Transfer held items from p2 to p1 */
            for (int i = 0; i < ctx->n_items; i++) {
                if (ctx->items[i].owner == (int8_t)sec_idx) {
                    ctx->items[i].owner = (int8_t)party_idx;
                }
            }

            /* Sacrifice secondary unit: remove p2 from ctx->party and update party & item indices */
            for (int i = sec_idx; i < ctx->n_party - 1; i++) {
                ctx->party[i] = ctx->party[i + 1];
            }
            ctx->n_party--;

            /* Update owner indices in ctx->items after removing sec_idx */
            for (int i = 0; i < ctx->n_items; i++) {
                if (ctx->items[i].owner > (int8_t)sec_idx) {
                    ctx->items[i].owner--;
                }
            }

            if (msg) *msg = NULL;
            return true;
        }

        default:
            if (msg) *msg = NO_EFFECT_MSG;
            return false;
    }
}
