#include "pr/reward.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static uint16_t parse_item_key(const char *key) {
    if (strcmp(key, "POTION") == 0) return PR_ITEM_POTION;
    if (strcmp(key, "SUPER_POTION") == 0) return PR_ITEM_SUPER_POTION;
    if (strcmp(key, "HYPER_POTION") == 0) return PR_ITEM_HYPER_POTION;
    if (strcmp(key, "MAX_POTION") == 0) return PR_ITEM_MAX_POTION;
    if (strcmp(key, "FULL_RESTORE") == 0) return PR_ITEM_FULL_RESTORE;
    if (strcmp(key, "REVIVE") == 0) return PR_ITEM_REVIVE;
    if (strcmp(key, "MAX_REVIVE") == 0) return PR_ITEM_MAX_REVIVE;
    if (strcmp(key, "SACRED_ASH") == 0) return PR_ITEM_SACRED_ASH;
    if (strcmp(key, "ETHER") == 0) return PR_ITEM_ETHER;
    if (strcmp(key, "MAX_ETHER") == 0) return PR_ITEM_MAX_ETHER;
    if (strcmp(key, "ELIXIR") == 0) return PR_ITEM_ELIXIR;
    if (strcmp(key, "MAX_ELIXIR") == 0) return PR_ITEM_MAX_ELIXIR;
    if (strcmp(key, "PP_UP") == 0) return PR_ITEM_PP_UP;
    if (strcmp(key, "PP_MAX") == 0) return PR_ITEM_PP_MAX;
    if (strcmp(key, "HEALING_CHARM") == 0) return PR_ITEM_HEALING_CHARM;
    if (strcmp(key, "MEGA_BRACELET") == 0) return PR_ITEM_MEGA_BRACELET;
    if (strcmp(key, "DYNAMAX_BAND") == 0) return PR_ITEM_DYNAMAX_BAND;
    if (strcmp(key, "EVOLUTION_ITEM") == 0) return PR_ITEM_EVOLUTION_ITEM;
    if (strcmp(key, "RARE_EVOLUTION_ITEM") == 0) return PR_ITEM_RARE_EVOLUTION_ITEM;
    if (strcmp(key, "FORM_CHANGE_ITEM") == 0) return PR_ITEM_FORM_CHANGE_ITEM;
    if (strcmp(key, "RARE_FORM_CHANGE_ITEM") == 0) return PR_ITEM_RARE_FORM_CHANGE_ITEM;
    if (strcmp(key, "DNA_SPLICERS") == 0) return PR_ITEM_DNA_SPLICERS;
    return 0;
}

static long num(char **p) {
    return strtol(*p, p, 10);
}

static void test_passive_multipliers(void) {
    pr_reward_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));

    /* EXP Share: zero stack or 0 participants */
    if (pr_reward_exp_share_multiplier(&ctx, 1) != 0.0) {
        fprintf(stderr, "FAIL: exp_share zero stack\n");
        exit(1);
    }
    ctx.items[0].item = PR_ITEM_EXP_SHARE;
    ctx.items[0].stack = 5;
    ctx.n_items = 1;
    if (pr_reward_exp_share_multiplier(&ctx, 0) != 0.0) {
        fprintf(stderr, "FAIL: exp_share 0 participants\n");
        exit(1);
    }
    if (pr_reward_exp_share_multiplier(&ctx, 2) != 0.5) { /* (5 * 0.2) / 2 = 0.5 */
        fprintf(stderr, "FAIL: exp_share 5 stack / 2 participants\n");
        exit(1);
    }

    /* EXP All / Oval Charm: multiple participant bonus */
    memset(&ctx, 0, sizeof(ctx));
    if (pr_reward_multiple_participant_exp_bonus(&ctx) != 0.0) {
        fprintf(stderr, "FAIL: multiple_participant_exp_bonus zero stack\n");
        exit(1);
    }
    ctx.items[0].item = PR_ITEM_OVAL_CHARM;
    ctx.items[0].stack = 5;
    ctx.n_items = 1;
    if (pr_reward_multiple_participant_exp_bonus(&ctx) != 1.0) { /* 5 * 0.2 = 1.0 */
        fprintf(stderr, "FAIL: multiple_participant_exp_bonus 5 stack\n");
        exit(1);
    }

    /* EXP Balance: lerp weight */
    memset(&ctx, 0, sizeof(ctx));
    if (pr_reward_exp_balance_weight(&ctx) != 0.0) {
        fprintf(stderr, "FAIL: exp_balance_weight zero stack\n");
        exit(1);
    }
    ctx.items[0].item = PR_ITEM_EXP_BALANCE;
    ctx.items[0].stack = 4;
    ctx.n_items = 1;
    if (pr_reward_exp_balance_weight(&ctx) != 0.8) { /* 0.2 * 4 = 0.8 */
        fprintf(stderr, "FAIL: exp_balance_weight 4 stack\n");
        exit(1);
    }

    /* Golden Punch: holder money multiplier */
    memset(&ctx, 0, sizeof(ctx));
    ctx.n_party = 2;
    ctx.party[0].id = 101;
    ctx.party[1].id = 202;
    if (pr_reward_golden_punch_money_multiplier(&ctx, 101) != 0.0) {
        fprintf(stderr, "FAIL: golden_punch zero stack\n");
        exit(1);
    }
    ctx.items[0].item = PR_ITEM_GOLDEN_PUNCH;
    ctx.items[0].stack = 5;
    ctx.items[0].owner = 1; /* owned by party index 1 (id 202) */
    ctx.n_items = 1;
    if (pr_reward_golden_punch_money_multiplier(&ctx, 101) != 0.0) {
        fprintf(stderr, "FAIL: golden_punch non-holder\n");
        exit(1);
    }
    if (pr_reward_golden_punch_money_multiplier(&ctx, 202) != 2.5) { /* 0.5 * 5 = 2.5 */
        fprintf(stderr, "FAIL: golden_punch holder\n");
        exit(1);
    }

    /* Amulet Coin: global money multiplier */
    memset(&ctx, 0, sizeof(ctx));
    if (pr_reward_money_multiplier(&ctx) != 1.0) {
        fprintf(stderr, "FAIL: money_multiplier zero stack\n");
        exit(1);
    }
    ctx.items[0].item = PR_ITEM_AMULET_COIN;
    ctx.items[0].stack = 5;
    ctx.n_items = 1;
    if (pr_reward_money_multiplier(&ctx) != 2.0) { /* 1.0 + 0.2 * 5 = 2.0 */
        fprintf(stderr, "FAIL: money_multiplier 5 stack\n");
        exit(1);
    }

    printf("PASS: passive_multipliers unit tests\n");
}

static void test_transfer_and_stealing(void) {
    pr_reward_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.n_party = 2;
    ctx.party[0].id = 101;
    ctx.party[1].id = 202;

    /* Give party[0] a Leftovers (PR_ITEM_LEFTOVERS, max_held 1) with stack 1 */
    ctx.items[0].item = PR_ITEM_LEFTOVERS;
    ctx.items[0].stack = 1;
    ctx.items[0].owner = 0;
    ctx.items[0].active = true;
    ctx.n_items = 1;

    /* Transfer from 0 to 1 with bypass_cap = false: should succeed */
    if (!pr_reward_try_transfer_held_item(&ctx, 0, 1, 1, false)) {
        fprintf(stderr, "FAIL: try_transfer_held_item basic transfer\n");
        exit(1);
    }
    if (ctx.items[0].owner != 1 || ctx.items[0].stack != 1) {
        fprintf(stderr, "FAIL: try_transfer_held_item owner/stack after transfer\n");
        exit(1);
    }

    /* Give party[0] another Leftovers, party[1] already has 4 Leftovers (max_held = 4) */
    ctx.items[0].owner = 1; /* index 0 is party[1]'s Leftovers */
    ctx.items[0].stack = 4;
    ctx.items[1].item = PR_ITEM_LEFTOVERS;
    ctx.items[1].stack = 1;
    ctx.items[1].owner = 0;
    ctx.items[1].active = true;
    ctx.n_items = 2;

    /* Transfer with bypass_cap = false should fail because target (1) has max_held (1) */
    if (pr_reward_try_transfer_held_item(&ctx, 1, 1, 1, false)) {
        fprintf(stderr, "FAIL: try_transfer_held_item cap enforcement\n");
        exit(1);
    }

    /* Transfer with bypass_cap = true should succeed and bypass cap (stack becomes 2) */
    if (!pr_reward_try_transfer_held_item(&ctx, 1, 1, 1, true)) {
        fprintf(stderr, "FAIL: try_transfer_held_item bypass cap\n");
        exit(1);
    }
    if (ctx.n_items != 1 || ctx.items[0].owner != 1 || ctx.items[0].stack != 5) {
        fprintf(stderr, "FAIL: try_transfer_held_item merged stack bypass cap\n");
        exit(1);
    }

    /* Test steal held item with item_idx = -1 */
    memset(&ctx, 0, sizeof(ctx));
    ctx.n_party = 2;
    ctx.items[0].item = PR_ITEM_GRIP_CLAW;
    ctx.items[0].stack = 1;
    ctx.items[0].owner = 0;
    ctx.items[0].active = true;
    ctx.n_items = 1;

    if (!pr_reward_steal_held_item(&ctx, 0, 1, -1, 1, true)) {
        fprintf(stderr, "FAIL: steal_held_item random pick\n");
        exit(1);
    }
    if (ctx.items[0].owner != 1) {
        fprintf(stderr, "FAIL: steal_held_item new owner\n");
        exit(1);
    }

    /* Test Black Sludge shop cost multiplier */
    memset(&ctx, 0, sizeof(ctx));
    if (pr_reward_black_sludge_shop_cost_multiplier(&ctx) != 1.0) {
        fprintf(stderr, "FAIL: black_sludge_shop_cost_multiplier without item\n");
        exit(1);
    }

    ctx.items[0].item = PR_ITEM_MYSTERY_ENCOUNTER_BLACK_SLUDGE;
    ctx.items[0].stack = 1;
    ctx.items[0].active = true;
    ctx.n_items = 1;
    if (pr_reward_black_sludge_shop_cost_multiplier(&ctx) != 2.5) {
        fprintf(stderr, "FAIL: black_sludge_shop_cost_multiplier with item\n");
        exit(1);
    }

    printf("PASS: transfer_and_stealing unit tests\n");
}

static void test_shop_economy_and_lock_capsule(void) {
    pr_reward_ctx ctx;
    pr_run run;
    pr_seedstr seed;
    memset(&run, 0, sizeof(run));
    pr_seed_from_ascii(&seed, "12345");
    pr_rng_sow(&run.rng, &seed);

    memset(&ctx, 0, sizeof(ctx));
    ctx.run = &run;
    ctx.wave = 1;

    /* 1. Basic reroll cost (unlocked, wave 1, reroll_count 0) */
    double cost = pr_reward_reroll_cost(&ctx, 0, false, NULL, 0, NULL);
    if (cost != 250.0) {
        fprintf(stderr, "FAIL: reroll_cost basic (got %.0f, expected 250)\n", cost);
        exit(1);
    }

    /* Scaling by reroll count */
    cost = pr_reward_reroll_cost(&ctx, 1, false, NULL, 0, NULL);
    if (cost != 500.0) {
        fprintf(stderr, "FAIL: reroll_cost reroll_count=1 (got %.0f, expected 500)\n", cost);
        exit(1);
    }

    cost = pr_reward_reroll_cost(&ctx, 2, false, NULL, 0, NULL);
    if (cost != 1000.0) {
        fprintf(stderr, "FAIL: reroll_cost reroll_count=2 (got %.0f, expected 1000)\n", cost);
        exit(1);
    }

    /* Scaling by wave (wave 15 -> ceil(15/10) = 2) */
    ctx.wave = 15;
    cost = pr_reward_reroll_cost(&ctx, 0, false, NULL, 0, NULL);
    if (cost != 500.0) {
        fprintf(stderr, "FAIL: reroll_cost wave 15 (got %.0f, expected 500)\n", cost);
        exit(1);
    }
    ctx.wave = 1;

    /* 2. Lock Capsule tier freezing cost */
    pr_reward_option options[3];
    memset(options, 0, sizeof(options));
    options[0].tier = MODIFIER_TIER_COMMON; /* 50 */
    options[1].tier = MODIFIER_TIER_GREAT;  /* 125 */
    options[2].tier = MODIFIER_TIER_ULTRA;  /* 300 */
    /* Total base_value = 475 */

    cost = pr_reward_reroll_cost(&ctx, 0, true, options, 3, NULL);
    if (cost != 475.0) {
        fprintf(stderr, "FAIL: reroll_cost locked (got %.0f, expected 475)\n", cost);
        exit(1);
    }

    cost = pr_reward_reroll_cost(&ctx, 1, true, options, 3, NULL);
    if (cost != 950.0) {
        fprintf(stderr, "FAIL: reroll_cost locked reroll_count=1 (got %.0f, expected 950)\n", cost);
        exit(1);
    }

    /* 3. Custom settings override */
    pr_reward_custom custom;
    memset(&custom, 0, sizeof(custom));
    custom.reroll_multiplier = -1.0; /* disabled */
    cost = pr_reward_reroll_cost(&ctx, 0, false, NULL, 0, &custom);
    if (cost != -1.0) {
        fprintf(stderr, "FAIL: reroll_cost disabled custom (got %.0f, expected -1)\n", cost);
        exit(1);
    }

    custom.reroll_multiplier = 0.5;
    cost = pr_reward_reroll_cost(&ctx, 0, false, NULL, 0, &custom);
    if (cost != 125.0) {
        fprintf(stderr, "FAIL: reroll_cost custom multiplier 0.5 (got %.0f, expected 125)\n", cost);
        exit(1);
    }

    /* 4. Black Sludge effect on reroll cost */
    ctx.items[0].item = PR_ITEM_MYSTERY_ENCOUNTER_BLACK_SLUDGE;
    ctx.items[0].stack = 1;
    ctx.items[0].active = true;
    ctx.n_items = 1;

    cost = pr_reward_reroll_cost(&ctx, 0, false, NULL, 0, NULL);
    if (cost != 625.0) { /* 250 * 2.5 = 625 */
        fprintf(stderr, "FAIL: reroll_cost with Black Sludge (got %.0f, expected 625)\n", cost);
        exit(1);
    }
    ctx.n_items = 0; /* reset items */

    /* 5. Lock Capsule tier extraction helper */
    int8_t locked_tiers[PR_REWARD_MAX_OPTIONS];
    int n_locked = pr_reward_get_locked_tiers(options, 3, locked_tiers);
    if (n_locked != 3 || locked_tiers[0] != MODIFIER_TIER_COMMON ||
        locked_tiers[1] != MODIFIER_TIER_GREAT || locked_tiers[2] != MODIFIER_TIER_ULTRA) {
        fprintf(stderr, "FAIL: get_locked_tiers\n");
        exit(1);
    }

    /* 6. Money deduction helper */
    double money = 1000.0;
    if (!pr_reward_try_deduct_money(&money, 250.0) || money != 750.0) {
        fprintf(stderr, "FAIL: try_deduct_money success\n");
        exit(1);
    }

    if (pr_reward_try_deduct_money(&money, 1000.0) || money != 750.0) {
        fprintf(stderr, "FAIL: try_deduct_money insufficient\n");
        exit(1);
    }

    if (pr_reward_try_deduct_money(&money, -1.0) || money != 750.0) {
        fprintf(stderr, "FAIL: try_deduct_money negative cost\n");
        exit(1);
    }

    /* 7. Shop reroll state machine transition */
    ctx.n_party = 1;
    ctx.party[0].species = SPECIES_ID_PIKACHU;
    ctx.party[0].allowed_in_challenge = true;

    pr_reward_select sel;
    memset(&sel, 0, sizeof(sel));
    pr_reward_thresholds th;
    pr_reward_option out_options[PR_REWARD_MAX_OPTIONS];

    money = 2000.0;
    int n_res = pr_reward_reroll(&ctx, &money, false, NULL, 0, &sel, &th, out_options);
    if (n_res <= 0 || money != 1750.0 || sel.reroll_count != 1) {
        fprintf(stderr, "FAIL: pr_reward_reroll transition (n_res=%d, money=%.0f, reroll_count=%d)\n",
                n_res, money, sel.reroll_count);
        exit(1);
    }

    /* Perform a locked reroll with Lock Capsule (reroll_count = 1, cost = 475 * 2 = 950) */
    n_res = pr_reward_reroll(&ctx, &money, true, options, 3, &sel, &th, out_options);
    if (n_res != 3 || money != 800.0 || sel.reroll_count != 2) { /* 1750 - 950 = 800 */
        fprintf(stderr, "FAIL: pr_reward_reroll locked transition (n_res=%d, money=%.0f, reroll_count=%d)\n",
                n_res, money, sel.reroll_count);
        exit(1);
    }

    /* Attempt reroll with insufficient money (800 < cost 475 * 4 = 1900) */
    n_res = pr_reward_reroll(&ctx, &money, true, options, 3, &sel, &th, out_options);
    if (n_res != -1 || money != 800.0 || sel.reroll_count != 2) {
        fprintf(stderr, "FAIL: pr_reward_reroll insufficient money (n_res=%d, money=%.0f)\n", n_res, money);
        exit(1);
    }

    printf("PASS: shop_economy_and_lock_capsule unit tests\n");
}

int main(int argc, char **argv) {
    test_passive_multipliers();
    test_transfer_and_stealing();
    test_shop_economy_and_lock_capsule();

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <cases file>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    char line[4096];
    int cases = 0, pass = 0, fail = 0;
    int item_counts[21] = {0};
    const char *item_keys[21] = {
        "POTION", "SUPER_POTION", "HYPER_POTION", "MAX_POTION", "FULL_RESTORE",
        "REVIVE", "MAX_REVIVE", "SACRED_ASH",
        "ETHER", "MAX_ETHER", "ELIXIR", "MAX_ELIXIR",
        "PP_UP", "PP_MAX",
        "EVOLUTION_ITEM", "RARE_EVOLUTION_ITEM",
        "FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM",
        "MEGA_BRACELET", "DYNAMAX_BAND", "DNA_SPLICERS"
    };

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'A') {
            cases++;
            char *p = line + 1;
            char key[64];
            sscanf(p, "%63s", key);
            while (*p && *p != ' ') p++;
            while (*p == ' ') p++;
            while (*p && *p != ' ') p++; /* skip key */

            int16_t arg = (int16_t)num(&p);
            int target_party_idx = (int)num(&p);
            int target_move_idx = (int)num(&p);
            uint32_t challenges = (uint32_t)num(&p);
            int n_party = (int)num(&p);

            uint16_t item_id = parse_item_key(key);
            for (int k = 0; k < 21; k++) {
                if (strcmp(key, item_keys[k]) == 0) {
                    item_counts[k]++;
                    break;
                }
            }

            pr_reward_ctx ctx;
            memset(&ctx, 0, sizeof(ctx));
            ctx.challenges = challenges;
            ctx.n_party = n_party;

            for (int i = 0; i < n_party; i++) {
                if (!fgets(line, sizeof(line), f) || line[0] != 'P') break;
                char *pp = line + 1;
                pr_reward_mon *m = &ctx.party[i];
                memset(m, 0, sizeof(*m));
                m->id = (uint32_t)num(&pp);
                m->species = (int16_t)num(&pp);
                m->form = (int16_t)num(&pp);
                m->fusion_species = (int16_t)num(&pp);
                m->fusion_form = (int16_t)num(&pp);
                m->gender = (int8_t)num(&pp);
                m->fusion_gender = (int8_t)num(&pp);
                m->level = (uint8_t)num(&pp);
                m->hp = (int32_t)num(&pp);
                m->max_hp = (int32_t)num(&pp);
                m->status = (uint8_t)num(&pp);
                m->nature = (uint8_t)num(&pp);
                m->tera_type = (int8_t)num(&pp);
                m->friendship = (uint8_t)num(&pp);
                m->ability = (uint16_t)num(&pp);
                m->cur_ability = (uint16_t)num(&pp);
                m->passive = (uint16_t)num(&pp);
                m->can_apply_ability = num(&pp) != 0;
                m->can_apply_passive = num(&pp) != 0;
                m->pause_evolutions = num(&pp) != 0;
                m->allowed_in_challenge = num(&pp) != 0;
                m->can_toxic = num(&pp) != 0;
                m->can_burn = num(&pp) != 0;
                m->has_learnable_level_moves = num(&pp) != 0;
                m->persistent_treasure = (int)num(&pp);

                m->n_moves = (uint8_t)num(&pp);
                for (int mv = 0; mv < m->n_moves; mv++) {
                    m->moves[mv].move = (uint16_t)num(&pp);
                    m->moves[mv].pp_used = (uint8_t)num(&pp);
                    m->moves[mv].pp_up = (uint8_t)num(&pp);
                    m->moves[mv].max_pp = (uint8_t)num(&pp);
                }

                m->n_base_moves = (uint8_t)num(&pp);
                for (int mv = 0; mv < m->n_base_moves; mv++) {
                    m->base_moves[mv] = (uint16_t)num(&pp);
                }
            }

            /* Read items */
            if (fgets(line, sizeof(line), f) && line[0] == 'I') {
                char *ip = line + 1;
                ctx.n_items = (int)num(&ip);
                for (int j = 0; j < ctx.n_items; j++) {
                    char ikey[64];
                    if (sscanf(ip, "%63s", ikey) != 1) break;
                    while (*ip && *ip == ' ') ip++;
                    while (*ip && *ip != ' ') ip++;
                    ctx.items[j].item = parse_item_key(ikey);
                    ctx.items[j].stack = (int32_t)num(&ip);
                    ctx.items[j].owner = (int8_t)num(&ip);
                    ctx.items[j].arg = (int16_t)num(&ip);
                }
            }

            /* Call C apply */
            const char *msg = NULL;
            bool res = pr_reward_apply(&ctx, item_id, arg, target_party_idx, target_move_idx, &msg);

            /* Read upstream result */
            if (!fgets(line, sizeof(line), f) || line[0] != 'R') {
                fail++;
                continue;
            }
            char *rp = line + 1;
            int exp_applied = (int)num(&rp);
            char exp_msg[128];
            sscanf(rp, "%127s", exp_msg);
            while (*rp && *rp != ' ') rp++;
            while (*rp == ' ') rp++;
            while (*rp && *rp != ' ') rp++;

            bool ok = (res == (exp_applied != 0));
            char diff_reason[256] = "";

            if (!ok) {
                snprintf(diff_reason, sizeof(diff_reason), "result mismatch: C res=%d, exp_applied=%d", res ? 1 : 0, exp_applied);
            } else {
                for (int i = 0; i < ctx.n_party && ok; i++) {
                    int exp_hp = (int)num(&rp);
                    int exp_status = (int)num(&rp);
                    int exp_n_moves = (int)num(&rp);
                    if (ctx.party[i].hp != exp_hp) {
                        ok = false;
                        snprintf(diff_reason, sizeof(diff_reason), "p[%d].hp: C=%d, exp=%d", i, ctx.party[i].hp, exp_hp);
                        break;
                    }
                    if (ctx.party[i].status != exp_status) {
                        ok = false;
                        snprintf(diff_reason, sizeof(diff_reason), "p[%d].status: C=%d, exp=%d", i, ctx.party[i].status, exp_status);
                        break;
                    }
                    if (ctx.party[i].n_moves != exp_n_moves) {
                        ok = false;
                        snprintf(diff_reason, sizeof(diff_reason), "p[%d].n_moves: C=%d, exp=%d", i, ctx.party[i].n_moves, exp_n_moves);
                        break;
                    }
                    for (int m = 0; m < ctx.party[i].n_moves; m++) {
                        int exp_pp_used = (int)num(&rp);
                        int exp_pp_up = (int)num(&rp);
                        int exp_max_pp = (int)num(&rp);
                        if (ctx.party[i].moves[m].pp_used != exp_pp_used ||
                            ctx.party[i].moves[m].pp_up != exp_pp_up ||
                            ctx.party[i].moves[m].max_pp != exp_max_pp) {
                            ok = false;
                            snprintf(diff_reason, sizeof(diff_reason), "p[%d].m[%d]: C=(used:%d,up:%d,max:%d), exp=(used:%d,up:%d,max:%d)",
                                     i, m, ctx.party[i].moves[m].pp_used, ctx.party[i].moves[m].pp_up, ctx.party[i].moves[m].max_pp,
                                     exp_pp_used, exp_pp_up, exp_max_pp);
                            break;
                        }
                    }
                }
            }

            if (ok) {
                pass++;
            } else {
                fail++;
                if (fail <= 10) {
                    printf("FAIL case %d (%s, arg=%d, target_p=%d, target_m=%d): %s\n", cases, key, arg, target_party_idx, target_move_idx, diff_reason);
                }
            }
        }
    }
    fclose(f);

    printf("== apply\n");
    printf("cases %d: pass %d, fail %d\n", cases, pass, fail);
    printf("cases per item:\n");
    for (int k = 0; k < 21; k++) {
        printf("  %-22s: %d\n", item_keys[k], item_counts[k]);
    }

    return fail == 0 ? 0 : 1;
}
