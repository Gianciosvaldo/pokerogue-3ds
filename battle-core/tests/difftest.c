#define _POSIX_C_SOURCE 200809L
/* C half of the differential test: replays cases written by
 * difftest.producer.test.ts (run inside upstream) and compares every answer.
 * A case whose computation touched an unported attribute is counted as
 * "not covered" (listed by class), never as a pass. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MON_FIELDS 47

static int read_mon(char **save, pr_pokemon *p)
{
    long long v[MON_FIELDS];
    for (int i = 0; i < MON_FIELDS; i++) {
        char *t = strtok_r(NULL, " \n", save);
        if (!t) return 0;
        v[i] = strtoll(t, NULL, 10);
    }
    memset(p, 0, sizeof *p);
    p->species = (uint16_t)v[0];
    p->form = (uint8_t)v[1];
    p->level = (uint8_t)v[2];
    for (int i = 0; i < 6; i++) p->ivs[i] = (uint8_t)v[3 + i];
    p->nature = (uint8_t)v[9];
    for (int i = 0; i < 7; i++) p->stat_stages[i] = (int8_t)v[10 + i];
    p->status = (uint8_t)v[17];
    p->terastallized = v[18];
    p->tera_type = (int8_t)v[19];
    p->hp = v[20];
    p->ability = (uint16_t)v[21];
    p->gender = (int8_t)v[22];
    p->is_boss = v[23];
    p->stellar_types_boosted = (uint32_t)v[24];
    p->summon_hit_count = (uint8_t)v[25];
    p->n_move_history = (uint8_t)v[26];
    p->n_attacks_received = (uint8_t)v[27];
    p->acted = v[28];
    p->stat_stages_decreased = v[29];
    p->joined_round = v[30];
    p->damage_taken = v[31];
    p->combining_pledge = (uint16_t)v[32];
    p->hit_count = (int8_t)v[33];
    p->hits_left = (int8_t)v[34];
    p->n_tags = (uint8_t)v[35];
    p->friendship = (uint8_t)v[36];
    p->has_trainer = v[37];
    for (int i = 0; i < 4; i++) {
        p->moves[i] = (uint16_t)v[38 + i];
        p->pp_used[i] = (uint8_t)v[42 + i];
    }
    p->id = (uint32_t)v[46];
    p->base_ability = ABILITY_ID_BALL_FETCH;
    p->type_override[0] = p->type_override[1] = -2;
    p->turn_move_effectiveness = -1; /* turnData.moveEffectiveness unset */
    return 1;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096], eline[4096];
    pr_battle bt;
    memset(&bt, 0, sizeof bt);
    int total = 0, covered = 0, pass = 0, fail = 0, stat_fail = 0;
    static int uncovered_by[PR_FEAT_COUNT_], deferred_by[PR_AC_COUNT_];
    int with_deferred = 0;
    pr_pokemon A, D;
    int mods_expected[2] = {0, 0};

    while (fgets(line, sizeof line, f)) {
        char *save;
        char *tag = strtok_r(line, " \n", &save);
        if (!tag) continue;
        if (!strcmp(tag, "Z")) {
            char *seed = strtok_r(NULL, " \n", &save);
            pr_seed_from_ascii(&bt.battle_seed, seed);
            bt.turn = atoi(strtok_r(NULL, " \n", &save));
            bt.battle_rng_live = false;
            continue;
        }
        if (!strcmp(tag, "W")) {
            int w[16];
            char *t;
            bt.turn = atoi(strtok_r(NULL, " \n", &save));
            pr_seed_from_ascii(&bt.wave_seed, strtok_r(NULL, " \n", &save));
            for (int i = 0; i < 15 && (t = strtok_r(NULL, " \n", &save)); i++) w[i] = atoi(t);
            bt.terrain = (uint8_t)w[0];
            bt.player_faints = w[1];
            bt.enemy_faints = w[2];
            bt.last_player_faint_turn = w[3];
            bt.last_enemy_faint_turn = w[4];
            bt.n_last_turn_order = (uint8_t)w[5];
            bt.n_arena_tags = w[6];
            mods_expected[0] = w[7];
            mods_expected[1] = w[8];
            bt.n_mods[0] = bt.n_mods[1] = 0;
            bt.max_exp_level = w[9];
            for (int i = 0; i < 4; i++) bt.turn_command[i] = (int8_t)w[10 + i];
            continue;
        }
        if (!strcmp(tag, "S")) {
            bt.battle_rng.c = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s0 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s1 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s2 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng_live = true;
            continue;
        }
        if (!strcmp(tag, "M")) {
            /* M side class-chain pokemonId stackCount virtualStackCount transferable stat statsMask mult
             *   moveType num nSpecies species... */
            int side = atoi(strtok_r(NULL, " \n", &save));
            if (bt.n_mods[side] >= PR_MAX_MODIFIERS) { fprintf(stderr, "too many modifiers\n"); return 2; }
            pr_modifier *m = &bt.mods[side][bt.n_mods[side]++];
            memset(m, 0, sizeof *m);
            char *chain = strtok_r(NULL, " \n", &save), *cs;
            int depth = 0;
            for (char *c = strtok_r(chain, "<", &cs); c; c = strtok_r(NULL, "<", &cs), depth++) {
                int k = pr_mod_class_by_name(c);
                if (k >= 0) { m->cls = (uint8_t)k; m->unknown_subclass = depth > 0; break; }
            }
            if (!m->cls) { fprintf(stderr, "modifier with no known class: %s\n", chain); return 2; }
            m->pokemon_id = strtoll(strtok_r(NULL, " \n", &save), NULL, 10);
            m->stack_count = atoi(strtok_r(NULL, " \n", &save));
            m->virtual_stack_count = atoi(strtok_r(NULL, " \n", &save));
            m->is_transferable = atoi(strtok_r(NULL, " \n", &save));
            m->stat = (int8_t)atoi(strtok_r(NULL, " \n", &save));
            m->stats_mask = (uint8_t)atoi(strtok_r(NULL, " \n", &save));
            m->mult = strtod(strtok_r(NULL, " \n", &save), NULL);
            m->move_type = (int8_t)atoi(strtok_r(NULL, " \n", &save));
            m->num = (int16_t)atoi(strtok_r(NULL, " \n", &save));
            int ns = atoi(strtok_r(NULL, " \n", &save));
            if (ns > 4) { fprintf(stderr, "species list too long\n"); return 2; }
            for (int i = 0; i < ns; i++) m->species[m->n_species++] = (uint16_t)atoi(strtok_r(NULL, " \n", &save));
            continue;
        }
        if (strcmp(tag, "C")) continue;
        if (bt.n_mods[0] != mods_expected[0] || bt.n_mods[1] != mods_expected[1]) {
            fprintf(stderr, "modifier count mismatch\n");
            return 2;
        }
        if (!read_mon(&save, &A) || !read_mon(&save, &D)) break;
        A.is_player = true;
        bt.weather = (uint8_t)atoi(strtok_r(NULL, " \n", &save));
        int move = atoi(strtok_r(NULL, " \n", &save));
        bool crit = atoi(strtok_r(NULL, " \n", &save));
        bool swap = atoi(strtok_r(NULL, " \n", &save));
        if (!fgets(eline, sizeof eline, f)) break;
        double e[17]; /* JS numbers, printed in shortest round-trip form */
        char *es;
        strtok_r(eline, " \n", &es);
        for (int i = 0; i < 17; i++) e[i] = strtod(strtok_r(NULL, " \n", &es), NULL);

        bt.field[0] = &A; bt.field[2] = &D;
        bt.n_unported = 0;
        bt.n_deferred = 0;
        total++;
        int32_t hp_a = A.hp, hp_d = D.hp;
        pr_calculate_stats(&bt, &A);
        pr_calculate_stats(&bt, &D);
        A.hp = hp_a;
        D.hp = hp_d;
        bool stats_ok = true;
        for (int i = 0; i < 6; i++)
            if (A.stats[i] != e[i] || D.stats[i] != e[6 + i]) stats_ok = false;
        if (!stats_ok) {
            stat_fail++;
            if (stat_fail <= 5) {
                printf("STATS MISMATCH species %d/%d lv %d nature %d: C", A.species, A.form, A.level, A.nature);
                for (int i = 0; i < 6; i++) printf(" %d", A.stats[i]);
                printf(" | JS");
                for (int i = 0; i < 6; i++) printf(" %.0f", e[i]);
                printf("\n");
            }
        }
        pr_pokemon *src = swap ? &D : &A, *def = swap ? &A : &D;
        pr_damage_result sim = pr_get_attack_damage(&bt, def, src, move, crit, true);
        bool c_crit = pr_get_critical_hit_result(&bt, def, src, move);
        pr_damage_result real = pr_get_attack_damage(&bt, def, src, move, crit, false);

        if (bt.n_unported) {
            for (int i = 0; i < bt.n_unported; i++) uncovered_by[bt.unported[i]]++;
            continue;
        }
        covered++;
        if (bt.n_deferred) with_deferred++;
        for (int i = 0; i < bt.n_deferred; i++) deferred_by[bt.deferred[i]]++;
        bool ok = stats_ok && sim.damage == e[12] && sim.result == e[13] && c_crit == (e[14] != 0)
                  && real.damage == e[15] && real.result == e[16];
        if (ok) pass++;
        else if (++fail <= 10)
            printf("MISMATCH move %d (%s) A %d/%d D %d/%d crit %d swap %d weather %d terrain %d: C sim %.17g/%d crit %d real %.17g/%d | JS %.17g/%.0f %.0f %.17g/%.0f\n",
                   move, pr_moves[move].name, A.species, A.form, D.species, D.form, crit, swap, bt.weather, bt.terrain,
                   sim.damage, sim.result, c_crit, real.damage, real.result, e[12], e[13], e[14], e[15], e[16]);
    }
    printf("cases %d, stats mismatches %d (all cases)\n", total, stat_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail, total - covered);
    printf("covered cases whose side effects were deferred to the (unported) phase engine: %d\n", with_deferred);
    for (int c = 0; c < PR_AC_COUNT_; c++)
        if (deferred_by[c]) printf("  deferred %s: %d\n", pr_attr_class_name[c], deferred_by[c]);
    printf("not covered because of (class or missing state: cases):\n");
    for (int pass_no = 0; pass_no < 30; pass_no++) {
        int best = -1;
        for (int c = 0; c < PR_FEAT_COUNT_; c++)
            if (uncovered_by[c] && (best < 0 || uncovered_by[c] > uncovered_by[best])) best = c;
        if (best < 0) break;
        printf("  %s: %d\n", pr_unported_name((uint16_t)best), uncovered_by[best]);
        uncovered_by[best] = 0;
    }
    return fail || stat_fail ? 1 : 0;
}
