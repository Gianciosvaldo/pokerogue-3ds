#define _POSIX_C_SOURCE 200809L
/* C half of run-core's wild enemy test: replays the cases written by wildenemy.producer.test.ts and
 * compares the first wild enemy's species, boss flag, shiny lock, its rolls up to the moveset and the RNG
 * state there. */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static double num(char **save) { return strtod(tok(save), NULL); }
static void read_state(char **save, pr_rng *r)
{
    memset(r, 0, sizeof *r);
    r->c = num(save); r->s0 = num(save); r->s1 = num(save); r->s2 = num(save);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "wildenemy-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int fusions = 0;
    int line_no = 0, cases = 0, pass = 0, fail = 0, uncovered = 0, threw = 0, bosses = 0, bugs = 0, locked = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (strcmp(t, "W")) continue;
        cases++;
        pr_run run;
        pr_wild_enemy_ctx ctx;
        memset(&run, 0, sizeof run);
        memset(&ctx, 0, sizeof ctx);
        run.mode = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));
        pr_seed_from_ascii(&run.wave_seed, tok(&save));
        ctx.enemy.wave = (int)num(&save);
        ctx.enemy.biome = (int)num(&save);
        ctx.enemy.wave_cycle_offset = (int)num(&save);
        run.offset_gym = num(&save) != 0;
        ctx.enemy.battle_type = BATTLE_TYPE_WILD;
        ctx.enemy.trainer_type = -1;
        ctx.level = (int)num(&save);
        ctx.luck = (int)num(&save);
        ctx.golden_bug_net = num(&save) != 0;
        ctx.enemy.hidden_ability_charm_stacks = (int)num(&save);
        ctx.enemy.shiny_charm_stacks = (int)num(&save);
        ctx.enemy.enemy_fusion_stacks = (int)num(&save);
        ctx.enemy.enemy_fusion_chance = num(&save);
        ctx.enemy.trainer_id = (uint16_t)num(&save);
        ctx.enemy.secret_id = (uint16_t)num(&save);
        read_state(&save, &run.rng);
        const int up_species = (int)num(&save);
        const bool up_boss = num(&save) != 0, up_lock = num(&save) != 0;
        char *first = tok(&save);
        pr_wild_enemy me;
        memset(&me, 0, sizeof me);
        int rc = pr_run_gen_wild_enemy(&run, &ctx, &me);
        if (run.unported) { uncovered++; continue; }
        bool ok = rc == 0 && strcmp(first, "none") && me.species == up_species && me.boss == up_boss
                  && me.shiny_lock == up_lock && me.mon.ability_index == atoi(first);
        if (ok) {
            ok = me.mon.id == (uint32_t)num(&save);
            ok = ok && me.mon.gender == (int)num(&save);
            ok = ok && me.mon.form == (int)num(&save);
            ok = ok && me.mon.nature == (int)num(&save);
            ok = ok && me.mon.tera_type == (int)num(&save);
            if (ok) {
                const int fs = (int)num(&save), fa = (int)num(&save), fg = (int)num(&save), ff = (int)num(&save);
                ok = me.mon.fusion_species == fs
                     && (fs < 0 || (me.mon.fusion_ability_index == fa && me.mon.fusion_gender == fg && me.mon.fusion_form == ff));
                fusions += ok && fs >= 0;
            }
            ok = ok && me.mon.boss_segments == (int)num(&save);
            pr_rng after;
            read_state(&save, &after);
            ok = ok && run.rng.c == after.c && run.rng.s0 == after.s0 && run.rng.s1 == after.s1 && run.rng.s2 == after.s2;
        }
        if (ok) {
            pass++;
            bosses += up_boss;
            locked += up_lock;
            bugs += ctx.golden_bug_net;
        } else if (++fail <= 10) {
            printf("MISMATCH line %d: species %d (upstream %d)\n", line_no, me.species, up_species);
        }
    }
    printf("cases %d (upstream threw: %d); checked: %d bosses, %d shiny-locked, %d with the Golden Bug Net, %d fusions\n",
           cases, threw, bosses, locked, bugs, fusions);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "first wild enemy (+ RNG)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
