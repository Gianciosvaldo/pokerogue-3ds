#define _POSIX_C_SOURCE 200809L
/* C half of the differential test: replays cases written by
 * difftest.producer.test.ts (run inside upstream) and compares every answer.
 * A case whose computation touched an unported attribute is counted as
 * "not covered" (listed by class), never as a pass. */
#include "pr/battle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_mon(char **save, pr_pokemon *p)
{
    int v[23];
    for (int i = 0; i < 23; i++) {
        char *t = strtok_r(NULL, " \n", save);
        if (!t) return 0;
        v[i] = atoi(t);
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
    p->base_ability = ABILITY_ID_BALL_FETCH;
    p->type_override[0] = p->type_override[1] = -2;
    p->turn_move_effectiveness = -1;
    p->hits_left = -1; /* turnData.moveEffectiveness unset */
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
    static int uncovered_by[PR_AC_COUNT_], deferred_by[PR_AC_COUNT_];
    int with_deferred = 0;
    pr_pokemon A, D;

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
        if (!strcmp(tag, "S")) {
            bt.battle_rng.c = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s0 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s1 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng.s2 = strtod(strtok_r(NULL, " \n", &save), NULL);
            bt.battle_rng_live = true;
            continue;
        }
        if (strcmp(tag, "C")) continue;
        if (!read_mon(&save, &A) || !read_mon(&save, &D)) break;
        A.is_player = true;
        bt.weather = (uint8_t)atoi(strtok_r(NULL, " \n", &save));
        int move = atoi(strtok_r(NULL, " \n", &save));
        bool crit = atoi(strtok_r(NULL, " \n", &save));
        if (!fgets(eline, sizeof eline, f)) break;
        long e[17];
        char *es;
        strtok_r(eline, " \n", &es);
        for (int i = 0; i < 17; i++) e[i] = atol(strtok_r(NULL, " \n", &es));

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
                for (int i = 0; i < 6; i++) printf(" %ld", e[i]);
                printf("\n");
            }
        }
        pr_damage_result sim = pr_get_attack_damage(&bt, &D, &A, move, crit, true);
        bool c_crit = pr_get_critical_hit_result(&bt, &D, &A, move);
        pr_damage_result real = pr_get_attack_damage(&bt, &D, &A, move, crit, false);

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
            printf("MISMATCH move %d (%s) A %d/%d D %d/%d crit %d weather %d: C sim %d/%d crit %d real %d/%d | JS %ld/%ld %ld %ld/%ld\n",
                   move, pr_moves[move].name, A.species, A.form, D.species, D.form, crit, bt.weather,
                   sim.damage, sim.result, c_crit, real.damage, real.result, e[12], e[13], e[14], e[15], e[16]);
    }
    printf("cases %d, stats mismatches %d (all cases)\n", total, stat_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail, total - covered);
    printf("covered cases whose side effects were deferred to the (unported) phase engine: %d\n", with_deferred);
    for (int c = 0; c < PR_AC_COUNT_; c++)
        if (deferred_by[c]) printf("  deferred %s: %d\n", pr_attr_class_name[c], deferred_by[c]);
    printf("not covered because of (class: cases):\n");
    for (int pass_no = 0; pass_no < 30; pass_no++) {
        int best = -1;
        for (int c = 0; c < PR_AC_COUNT_; c++)
            if (uncovered_by[c] && (best < 0 || uncovered_by[c] > uncovered_by[best])) best = c;
        if (best < 0) break;
        printf("  %s: %d\n", pr_attr_class_name[best], uncovered_by[best]);
        uncovered_by[best] = 0;
    }
    return fail || stat_fail ? 1 : 0;
}
