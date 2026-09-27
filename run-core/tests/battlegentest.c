#define _POSIX_C_SOURCE 200809L
/* C half of run-core's new-battle generation test: replays battlegen.producer.test.ts's cases through
 * pr_run_new_battle and compares everything upstream's newBattle built, and the RNG state after it. */
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

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "battlegen-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[2048];
    int pass = 0, fail = 0, uncovered = 0, line_no = 0, kinds[4] = {0}, doubles = 0;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || strcmp(t, "G")) continue;
        pr_run run;
        memset(&run, 0, sizeof run);
        pr_new_battle_ctx ctx;
        memset(&ctx, 0, sizeof ctx);
        run.mode = (int)num(&save);
        pr_seed_from_ascii(&run.seed, tok(&save));
        run.offset_gym = num(&save) != 0;
        ctx.prev_wave = (int)num(&save);
        ctx.biome = (int)num(&save);
        ctx.trainer_chance = (int)num(&save);
        ctx.n_lures = (int)num(&save);
        ctx.me.spawn_chance = num(&save);
        ctx.me.n_encountered = (int)num(&save);
        ctx.me.last_encounter_wave = (int)num(&save);
        int wave = (int)num(&save);
        pr_new_battle js;
        memset(&js, 0, sizeof js);
        js.battle_type = (int)num(&save);
        js.double_battle = num(&save) != 0;
        js.trainer_type = (int)num(&save);
        js.variant = (int)num(&save);
        js.template_index = (int)num(&save);
        js.name = (int)num(&save);
        js.partner_name = (int)num(&save);
        js.n_levels = (int)num(&save);
        if (js.n_levels < 0 || js.n_levels > PR_MAX_PARTY_TEMPLATE) { fprintf(stderr, "bad levels\n"); return 2; }
        for (int i = 0; i < js.n_levels; i++) js.levels[i] = (int)num(&save);
        double chance_after = num(&save);
        char js_seed[32];
        snprintf(js_seed, sizeof js_seed, "%s", tok(&save));
        pr_rng after;
        memset(&after, 0, sizeof after);
        after.c = num(&save); after.s0 = num(&save); after.s1 = num(&save); after.s2 = num(&save);

        pr_new_battle mine;
        if (pr_run_new_battle(&run, wave, &ctx, &mine) < 0 || run.unported) { uncovered++; continue; }
        double my_chance = mine.me_spawn_chance_reset ? 3 : ctx.me.spawn_chance;   /* BASE_MYSTERY_ENCOUNTER_SPAWN_WEIGHT */
        bool ok = mine.battle_type == js.battle_type && mine.double_battle == js.double_battle
                  && mine.trainer_type == js.trainer_type && mine.variant == js.variant
                  && mine.template_index == js.template_index && mine.name == js.name && mine.partner_name == js.partner_name
                  && mine.n_levels == js.n_levels && my_chance == chance_after && !strcmp(mine.battle_seed, js_seed)
                  && run.rng.c == after.c
                  && run.rng.s0 == after.s0 && run.rng.s1 == after.s1 && run.rng.s2 == after.s2;
        for (int i = 0; ok && i < js.n_levels; i++) ok = mine.levels[i] == js.levels[i];
        if (ok) {
            pass++;
            if (js.battle_type >= 0 && js.battle_type < 4) kinds[js.battle_type]++;
            doubles += js.double_battle;
        } else if (++fail <= 10) {
            printf("MISMATCH line %d: type %d/%d double %d/%d trainer %d/%d variant %d/%d template %d/%d names %d,%d/%d,%d levels",
                   line_no, mine.battle_type, js.battle_type, mine.double_battle, js.double_battle, mine.trainer_type,
                   js.trainer_type, mine.variant, js.variant, mine.template_index, js.template_index, mine.name,
                   mine.partner_name, js.name, js.partner_name);
            for (int i = 0; i < js.n_levels || i < mine.n_levels; i++)
                printf(" %d/%d", i < mine.n_levels ? mine.levels[i] : -1, i < js.n_levels ? js.levels[i] : -1);
            printf("\n");
        }
    }
    printf("newBattle: pass %d  fail %d  not covered %d (wild %d, trainer %d, mystery encounter %d; doubles %d)\n", pass,
           fail, uncovered, kinds[0], kinds[1], kinds[3], doubles);
    return fail ? 1 : 0;
}
