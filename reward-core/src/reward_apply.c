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

        default:
            if (msg) *msg = NO_EFFECT_MSG;
            return false;
    }
}

/* ---- passive inventory multipliers (EXP Items & Economy) ---- */

static int get_item_stack(const pr_reward_ctx *ctx, uint16_t item) {
    if (!ctx) return 0;
    for (int i = 0; i < ctx->n_items; i++) {
        if (ctx->items[i].item == item && ctx->items[i].stack > 0) {
            return ctx->items[i].stack;
        }
    }
    return 0;
}

static int get_held_item_stack(const pr_reward_ctx *ctx, uint16_t item, uint32_t pokemon_id) {
    if (!ctx) return 0;
    for (int i = 0; i < ctx->n_party; i++) {
        if (ctx->party[i].id == pokemon_id) {
            for (int j = 0; j < ctx->n_items; j++) {
                if (ctx->items[j].item == item && ctx->items[j].owner == i && ctx->items[j].stack > 0) {
                    return ctx->items[j].stack;
                }
            }
            break;
        }
    }
    return 0;
}

double pr_reward_exp_share_multiplier(const pr_reward_ctx *ctx, int n_participants) {
    if (n_participants <= 0) return 0.0;
    int stack = get_item_stack(ctx, PR_ITEM_EXP_SHARE);
    if (stack <= 0) return 0.0;
    return (stack * 0.2) / (double)n_participants;
}

double pr_reward_multiple_participant_exp_bonus(const pr_reward_ctx *ctx) {
    int stack = get_item_stack(ctx, PR_ITEM_OVAL_CHARM);
    if (stack <= 0) return 0.0;
    return stack * 0.2;
}

double pr_reward_exp_balance_weight(const pr_reward_ctx *ctx) {
    int stack = get_item_stack(ctx, PR_ITEM_EXP_BALANCE);
    if (stack <= 0) return 0.0;
    return 0.2 * stack;
}

double pr_reward_golden_punch_money_multiplier(const pr_reward_ctx *ctx, uint32_t pokemon_id) {
    int stack = get_held_item_stack(ctx, PR_ITEM_GOLDEN_PUNCH, pokemon_id);
    if (stack <= 0) return 0.0;
    return 0.5 * stack;
}

double pr_reward_money_multiplier(const pr_reward_ctx *ctx) {
    int stack = get_item_stack(ctx, PR_ITEM_AMULET_COIN);
    return 1.0 + 0.2 * stack;
}
