/* Test comparator for trainer rewards & genModifiersFunc trace dumps */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "pr/reward.h"
#include "pr_enums.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <trace_file> [--mutate]\n", argv[0]);
        return 1;
    }

    bool mutate = (argc >= 3 && strcmp(argv[2], "--mutate") == 0);

    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror("fopen");
        return 1;
    }

    char line[4096];
    int cases = 0, mismatches = 0;

    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == 'X') continue;
        if (line[0] != 'T') continue;

        cases++;
        int trainer_type = 0;
        int is_boss = 0;
        int has_voucher_config = 0;
        double money_mult = 1.0;
        int n_reward_keys = 0;
        char reward_keys[8][128];
        char voucher_key[128] = "NONE";
        int is_guts = 0, is_ph2 = 0, is_ph4 = 0, is_ms = 0;
        int n_gen_keys = 0;
        char gen_keys[8][128];
        int wave = 0;
        char custom_str[256] = "NONE";
        char rng_state[256] = "0";

        /* Parse line: T <trainer_type> <is_boss> <has_voucher_config> <money_mult> <n_reward_keys> ... */
        char *ptr = line + 2;
        int read_bytes = 0;
        if (sscanf(ptr, "%d %d %d %lf %d%n", &trainer_type, &is_boss, &has_voucher_config, &money_mult, &n_reward_keys, &read_bytes) != 5) {
            continue;
        }
        ptr += read_bytes;

        for (int i = 0; i < n_reward_keys; i++) {
            if (sscanf(ptr, " %127s%n", reward_keys[i], &read_bytes) != 1) break;
            ptr += read_bytes;
        }

        if (sscanf(ptr, " %127s %d %d %d %d %d%n", voucher_key, &is_guts, &is_ph2, &is_ph4, &is_ms, &n_gen_keys, &read_bytes) != 6) {
            continue;
        }
        ptr += read_bytes;

        for (int i = 0; i < n_gen_keys; i++) {
            if (sscanf(ptr, " %127s%n", gen_keys[i], &read_bytes) != 1) break;
            ptr += read_bytes;
        }

        if (sscanf(ptr, " %d %255s %255s", &wave, custom_str, rng_state) != 3) {
            continue;
        }

        if (mutate) {
            trainer_type = 0;
        }

        /* 1. Compare trainer victory rewards */
        uint16_t out_items[8];
        bool has_voucher_unlocked = (strcmp(voucher_key, "NONE") == 0);
        bool upgrade_unlocked = (strcmp(voucher_key, "VOUCHER_PREMIUM") == 0 || strcmp(voucher_key, "VOUCHER_PLUS") == 0);

        int n_out = pr_reward_trainer_victory_rewards(trainer_type, is_boss != 0, has_voucher_config != 0, has_voucher_unlocked, upgrade_unlocked, money_mult, out_items);

        int expected_rewards = n_reward_keys + (strcmp(voucher_key, "NONE") != 0 ? 1 : 0);
        if (n_out != expected_rewards) {
            if (mismatches < 5) {
                printf("Mismatch case %d (victory rewards): trainer %d is_boss %d exp_keys %d voucher %s -> n_out %d\n",
                       cases, trainer_type, is_boss, n_reward_keys, voucher_key, n_out);
            }
            mismatches++;
            continue;
        }

        /* 2. Compare fixed battle custom settings */
        pr_reward_custom custom;
        int has_custom = pr_reward_fixed_battle_custom_settings(wave, &custom);
        bool exp_custom = (strcmp(custom_str, "NONE") != 0);
        if ((has_custom != 0) != exp_custom) {
            if (mismatches < 5) {
                printf("Mismatch case %d (custom settings): wave %d exp_custom %d -> has_custom %d\n",
                       cases, wave, exp_custom, has_custom);
            }
            mismatches++;
            continue;
        }

        /* 3. Compare genModifiersFunc */
        pr_reward_mon party[6];
        memset(party, 0, sizeof(party));
        party[0].species = 25; /* Pikachu default */

        /* Mock party attributes based on trainer_type and parsed flags */
        if (trainer_type == TRAINER_TYPE_RED) {
            party[0].species = 25; /* PIKACHU */
        } else if (trainer_type == TRAINER_TYPE_STEVEN) {
            party[0].species = 526; /* GIGALITH */
        } else if (trainer_type == TRAINER_TYPE_WALLACE) {
            party[0].species = 279; /* PELIPPER */
            party[5].species = 350; /* MILOTIC */
            if (is_ms) party[5].ability = 63; /* MARVEL_SCALE */
        } else if (trainer_type == TRAINER_TYPE_CYNTHIA || trainer_type == TRAINER_TYPE_KIERAN) {
            party[2].species = 472; /* GLISCOR */
            if (is_ph2) party[2].ability = 90; /* POISON_HEAL */
        } else if (trainer_type == TRAINER_TYPE_RIVAL_5 || trainer_type == TRAINER_TYPE_RIVAL_6) {
            party[1].species = 277; /* SWELLOW */
            if (is_guts) party[1].ability = 62; /* GUTS */
        } else if (trainer_type == TRAINER_TYPE_GUZMA || trainer_type == TRAINER_TYPE_GUZMA_2 || trainer_type == TRAINER_TYPE_SKULL_GRUNT) {
            party[4].species = 472; /* GLISCOR */
            if (is_ph4) party[4].ability = 90; /* POISON_HEAL */
        }

        pr_item_type gen_out[8];
        int n_gen = pr_reward_trainer_gen_modifiers(trainer_type, party, 6, gen_out);
        if (n_gen != n_gen_keys) {
            if (mismatches < 5) {
                printf("Mismatch case %d (genModifiers): trainer %d exp_gen %d -> n_gen %d\n",
                       cases, trainer_type, n_gen_keys, n_gen);
            }
            mismatches++;
            continue;
        }
    }

    fclose(f);
    printf("cases %d: mismatches %d\n", cases, mismatches);
    return mismatches > 0 ? 1 : 0;
}
