#include "pr/reward.h"
#include "reward_internal.h"
#include "pr_data.h"
#include "pr/battle.h"
#include <string.h>
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

/* Status = null: the Status object's counters go with it (pr_pokemon's convention for no status) */
static void drop_status(pr_reward_mon *p) {
    p->status = 0;
    p->status_toxic_turns = 0;
    p->status_sleep_turns = p->status_freeze_turns = -1;
}

/* fuse(): this.status = pokemon.status moves the Status object, counters included */
static void take_status(pr_reward_mon *to, const pr_reward_mon *from) {
    to->status = from->status;
    to->status_toxic_turns = from->status_toxic_turns;
    to->status_sleep_turns = from->status_sleep_turns;
    to->status_freeze_turns = from->status_freeze_turns;
}

/* upstream Pokemon.resetStatus(true, true, false, false) -> clearStatus(true, false) (pokemon.ts):
 *   lastStatus = status?.effect; status = null;
 *   if (lastStatus === SLEEP && getTag(NIGHTMARE)) lapseTag(NIGHTMARE);
 *   if (confusion && getTag(CONFUSED)) lapseTag(CONFUSED);
 * lapseTag(tag) = tag.lapse(this, CUSTOM), and the tag is removed when that returns false. Nightmare's
 * lapse(CUSTOM) is --turnCount > 0 with turnCount 1: false, no text. ConfusedTag.lapse(CUSTOM) only ticks on
 * PRE_MOVE: false, so the tag goes and onRemove queues "battlerTags:confusedOnRemove". */
static void reset_status(pr_reward_ctx *ctx, int party_idx) {
    pr_reward_mon *p = &ctx->party[party_idx];
    const bool was_asleep = p->status == STATUS_EFFECT_SLEEP;
    drop_status(p);
    if (was_asleep && p->nightmare) p->nightmare = false;
    if (p->confused) {
        p->confused = false;
        if (ctx->n_apply_texts < PR_REWARD_MAX_TEXTS) {
            ctx->apply_texts[ctx->n_apply_texts].key = "battlerTags:confusedOnRemove";
            ctx->apply_texts[ctx->n_apply_texts].party_idx = (int8_t)party_idx;
            ctx->n_apply_texts++;
        }
    }
}

bool pr_reward_apply(pr_reward_ctx *ctx, uint16_t item, int16_t arg, int party_idx, int move_idx, const char **msg) {
    if (!ctx) {
        if (msg) *msg = NO_EFFECT_MSG;
        return false;
    }

    ctx->n_apply_texts = 0;
    const bool hardcore = (ctx->challenges & (1u << CHALLENGES_HARDCORE)) != 0;

    switch (item) {
        case PR_ITEM_BASE_STAT_BOOSTER:
            if (pr_reward_apply_vitamin(ctx, arg, party_idx, -1, NULL) != PR_REWARD_VITAMIN_APPLIED) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            if (msg) *msg = NULL;
            return true;

        case PR_ITEM_MINT:
            if (!pr_reward_apply_mint(ctx, arg, party_idx)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            if (msg) *msg = NULL;
            return true;

        case PR_ITEM_RARE_CANDY:
        case PR_ITEM_RARER_CANDY:
            if (!pr_reward_apply_candy(ctx, item, party_idx)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            if (msg) *msg = NULL;
            return true;

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

            /* PokemonHpRestoreModifierType's filter: !hp || (isFullHp() && (!healStatus ||
             * (!status && !getTag(CONFUSED)))) */
            if (p->hp >= p->max_hp) {
                if (!heal_status || (p->status == 0 && !p->confused)) {
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
                reset_status(ctx, party_idx);
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
            drop_status(p);

            int percent_pts = (int)floor((double)restore_pct * 0.01 * (double)p->max_hp);
            if (percent_pts < 1) percent_pts = 1;
            p->hp = percent_pts;
            if (p->hp > p->max_hp) p->hp = p->max_hp;

            if (msg) *msg = NULL;
            return true;
        }

        case PR_ITEM_FULL_HEAL: {
            if (party_idx < 0 || party_idx >= ctx->n_party) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_mon *p = &ctx->party[party_idx];
            /* PokemonStatusHealModifierType's filter: !hp || (!status && !getTag(CONFUSED)) */
            if (p->hp <= 0 || (p->status == 0 && !p->confused)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }

            /* PokemonStatusHealModifier.apply: resetStatus(true, true, false, false) */
            reset_status(ctx, party_idx);

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
                    drop_status(&ctx->party[i]);
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
            const pr_item_evo *matching = NULL;
            bool for_fusion = false;
            /* Selection checks the primary before the fusion. pauseEvolutions limits
             * offered item generation, not an explicit EvolutionItemModifierType
             * selection or EvolutionItemModifier.apply. Preserve registry order. */
            for (int fusion = 0; fusion < 2 && !matching; fusion++) {
                int species = fusion ? m->fusion_species : m->species;
                int form = fusion ? m->fusion_form : m->form;
                const pr_reward_species *s = pr_reward_species_find(species);
                if (!s || !(s->flags & 1) || pr_reward_form_is_max(species, form)) continue;
                const pr_species_slice *sl = pr_reward_slice_find(pr_item_evo_species, pr_item_evo_species_count, species);
                if (!sl) continue;
                int key = pr_reward_form_key(species, form);
                for (int i = 0; i < sl->n; i++) {
                    const pr_item_evo *e = &pr_item_evos[sl->start + i];
                    if (e->item == arg && m->level >= e->level
                        && (e->pre_form_key < 0 || e->pre_form_key == key)
                        && pr_reward_evo_conditions_ex(ctx, party_idx, e, fusion != 0)) {
                        matching = e;
                        for_fusion = fusion != 0;
                        break;
                    }
                }
            }
            if (!matching) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            /* Applying the item queues EvolutionPhase; it does not evolve the mon.
             * The caller owns phase execution, stat/move/dex updates and persistence. */
            if (ctx->n_evolution_events >= PR_REWARD_MAX_EVOLUTION_EVENTS) {
                ctx->evolution_events_overflow = true;
                if (msg) *msg = NO_EFFECT_MSG;
                return false;
            }
            pr_reward_evolution_event *event = &ctx->evolution_events[ctx->n_evolution_events++];
            *event = (pr_reward_evolution_event){
                .pokemon_id = m->id, .primary_species = m->species, .target_species = matching->species,
                .item = matching->item, .pre_form_key = matching->pre_form_key,
                .evo_form_key = matching->evo_form_key, .last_level = (int16_t)(m->level - 1),
                .evolution_level = matching->level, .fusion = for_fusion,
            };
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
                /* Item selection only matches preform and item; activation conditions belong to the later phase. */

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

            if (hardcore && ctx->party[party_idx].hp <= 0) { if (msg) *msg = NO_EFFECT_MSG; return false; }
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

            bool first_valid_status = p1->status != STATUS_EFFECT_NONE && p1->status != STATUS_EFFECT_FAINT;
            bool second_valid_status = p2->status != STATUS_EFFECT_NONE && p2->status != STATUS_EFFECT_FAINT;
            int original_max_hp = p1->max_hp;
            double p1_hp_pct = p1->max_hp > 0 ? (double)p1->hp / p1->max_hp : 0.0;
            double p2_hp_pct = p2->max_hp > 0 ? (double)p2->hp / p2->max_hp : 0.0;
            double new_hp_pct = (p1_hp_pct + p2_hp_pct) / 2.0;
            int new_hp = (int)floor((double)original_max_hp * new_hp_pct + 0.5);
            if (new_hp > 0 && first_valid_status && second_valid_status
                && (!ctx->fusion_status_choice_available || ctx->fusion_status_choice > 1)) {
                if (msg) *msg = NO_EFFECT_MSG;
                return false; /* refuse missing randomness input before mutation */
            }

            /* Perform fusion */
            p1->fusion_species = p2->species;
            p1->fusion_form = p2->form;
            p1->fusion_gender = p2->gender;
            if (p2->pause_evolutions) p1->pause_evolutions = true;

            /* Sparse form lookup, fusion ceil-average, IVs/nature and Wonder Guard
             * use the already ported battle stat calculator read-only. */
            pr_battle stat_bt = {0};
            pr_pokemon stat_mon = {0};
            stat_mon.species=p1->species;stat_mon.form=p1->form;
            stat_mon.fusion_species=p2->species;stat_mon.fusion_form=p2->form;
            stat_mon.level=p1->level;stat_mon.nature=p1->nature;
            stat_mon.base_ability=p2->ability;stat_mon.passive=p1->passive;
            stat_mon.is_player=true;stat_mon.id=p1->id;
            memcpy(stat_mon.ivs,p1->ivs,sizeof stat_mon.ivs);
            pr_calculate_stats(&stat_bt,&stat_mon);
            p1->max_hp=stat_mon.stats[STAT_HP];
            memcpy(p1->stats,stat_mon.stats,sizeof p1->stats);
            p1->fusion_ability_index=p2->ability_index;
            p1->fusion_shiny=p2->shiny;p1->fusion_variant=p2->variant;p1->fusion_luck=p2->luck;

            p1->hp = new_hp;
            if (p1->hp > 0) {
                if (p1->hp > original_max_hp) p1->hp = original_max_hp;
                if (!first_valid_status) take_status(p1, p2);
                else if (second_valid_status && !ctx->fusion_status_choice) take_status(p1, p2);
            } else if (p2->hp > 0) {
                if (p1->hp < 1) p1->hp = 1;
                take_status(p1, p2);
            }

            /* updateModifiers recalculates the new fusion maximum after assigning old-max HP. */
            if (p1->hp > p1->max_hp) p1->hp=p1->max_hp;

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

/* ---- inventory item transfer & stealing mechanics (Grip Claw, Mini Black Hole, Black Sludge) ---- */

static int get_max_held_count(const pr_reward_ctx *ctx, int to_p, uint16_t item_id, int16_t arg) {
    (void)to_p;
    if (item_id >= PR_ITEM_COUNT_) return 1;
    if (pr_items[item_id].gen) {
        for (int i = 0; i < pr_gen_type_count; i++) {
            if (pr_gen_types[i].item == item_id && pr_gen_types[i].arg == arg) {
                if (pr_gen_types[i].max_held >= 0) return pr_gen_types[i].max_held;
                break;
            }
        }
    }
    if (pr_items[item_id].max_held >= 0) {
        return pr_items[item_id].max_held;
    }
    if (pr_items[item_id].max_held == -2) {
        return 31;
    }
    if (pr_items[item_id].max_stack_threshold > 0) {
        return pr_items[item_id].max_stack_threshold;
    }
    return 1;
}

bool pr_reward_try_transfer_held_item(pr_reward_ctx *ctx, int item_idx, int to_p, int quantity, bool bypass_cap) {
    if (!ctx || item_idx < 0 || item_idx >= ctx->n_items || to_p < 0 || to_p >= ctx->n_party) {
        return false;
    }

    pr_owned_item *item = &ctx->items[item_idx];
    if (item->owner == (int8_t)to_p || item->stack <= 0) {
        return false;
    }

    uint16_t item_id = item->item;
    int16_t arg = item->arg;
    int transfer_qty = (quantity <= 0) ? item->stack : (quantity < item->stack ? quantity : item->stack);
    if (transfer_qty <= 0) return false;

    int max_held = bypass_cap ? 999999 : get_max_held_count(ctx, to_p, item_id, arg);

    /* Find matching item owned by to_p */
    int matching_idx = -1;
    for (int j = 0; j < ctx->n_items; j++) {
        if (ctx->items[j].owner == (int8_t)to_p && ctx->items[j].item == item_id && ctx->items[j].arg == arg) {
            matching_idx = j;
            break;
        }
    }

    if (matching_idx >= 0) {
        pr_owned_item *matching = &ctx->items[matching_idx];
        if (!bypass_cap && matching->stack >= max_held) {
            return false;
        }
        int space = bypass_cap ? transfer_qty : (max_held - matching->stack);
        int count_taken = (transfer_qty < space) ? transfer_qty : space;
        if (count_taken <= 0) return false;

        item->stack -= count_taken;
        matching->stack += count_taken;
    } else {
        int space = bypass_cap ? transfer_qty : max_held;
        int count_taken = (transfer_qty < space) ? transfer_qty : space;
        if (count_taken <= 0) return false;

        item->stack -= count_taken;
        if (item->stack == 0) {
            /* Reuse existing slot */
            item->owner = (int8_t)to_p;
            item->stack = count_taken;
        } else {
            /* Create new slot for target */
            if (ctx->n_items >= PR_REWARD_MAX_OWNED) {
                item->stack += count_taken;
                return false;
            }
            ctx->items[ctx->n_items].item = item_id;
            ctx->items[ctx->n_items].arg = arg;
            ctx->items[ctx->n_items].stack = count_taken;
            ctx->items[ctx->n_items].owner = (int8_t)to_p;
            ctx->items[ctx->n_items].battle_count = 0;
            ctx->items[ctx->n_items].active = true;
            ctx->n_items++;
        }
    }

    /* Clean up 0-stack items */
    int write_idx = 0;
    for (int i = 0; i < ctx->n_items; i++) {
        if (ctx->items[i].stack > 0) {
            if (write_idx != i) {
                ctx->items[write_idx] = ctx->items[i];
            }
            write_idx++;
        }
    }
    ctx->n_items = write_idx;

    return true;
}

bool pr_reward_steal_held_item(pr_reward_ctx *ctx, int from_p, int to_p, int item_idx, int quantity, bool bypass_cap) {
    if (!ctx || from_p < 0 || from_p >= ctx->n_party || to_p < 0 || to_p >= ctx->n_party || from_p == to_p) {
        return false;
    }

    if (item_idx >= 0) {
        if (item_idx >= ctx->n_items || ctx->items[item_idx].owner != (int8_t)from_p) {
            return false;
        }
    } else {
        int candidates[PR_REWARD_MAX_OWNED];
        int n_cand = 0;
        for (int i = 0; i < ctx->n_items; i++) {
            if (ctx->items[i].owner == (int8_t)from_p && ctx->items[i].stack > 0) {
                uint16_t it = ctx->items[i].item;
                if (it < PR_ITEM_COUNT_ && (pr_items[it].flags & PR_ITEMF_HELD)) {
                    candidates[n_cand++] = i;
                }
            }
        }
        if (n_cand == 0) return false;

        int chosen = 0;
        if (ctx->run) {
            chosen = pr_rng_seed_int(&ctx->run->rng, n_cand, 0);
        }
        item_idx = candidates[chosen];
    }

    return pr_reward_try_transfer_held_item(ctx, item_idx, to_p, quantity, bypass_cap);
}

double pr_reward_black_sludge_shop_cost_multiplier(const pr_reward_ctx *ctx) {
    if (!ctx) return 1.0;
    for (int i = 0; i < ctx->n_items; i++) {
        if (ctx->items[i].item == PR_ITEM_MYSTERY_ENCOUNTER_BLACK_SLUDGE && ctx->items[i].stack > 0 && ctx->items[i].active) {
            return 2.5;
        }
    }
    return 1.0;
}
