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

int main(int argc, char **argv) {
    test_passive_multipliers();

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
