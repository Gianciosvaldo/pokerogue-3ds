#define _POSIX_C_SOURCE 200809L
/* C half of run-core's enemy Pokemon test: replays the cases written by pokemongen.producer.test.ts and
 * compares the rolls before generateAndPopulateMoveset (ability, id, IVs, gender, form, nature, tera
 * type, boss segments, RNG state) and after it (shiny, variant, luck, friendship, final IVs, RNG state). */
#include "pr/run.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { C_BASE, C_FINISH, C_COUNT_ };
static const char *const check_names[C_COUNT_] = {
    "constructor up to the moveset (+ RNG)", "constructor after it, addEnemyPokemon (+ RNG)",
};
static int pass[C_COUNT_], fail[C_COUNT_], uncovered[C_COUNT_];

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
static bool same_state(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2;
}
static void record(int check, bool ok, int line_no)
{
    if (ok) { pass[check]++; return; }
    if (++fail[check] <= 10) printf("MISMATCH line %d: %s\n", line_no, check_names[check]);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "pokemongen-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int line_no = 0, cases = 0, threw = 0, shiny = 0, variants = 0, bosses = 0, trainer_mons = 0;
    pr_run run;
    pr_enemy_gen_ctx ctx;
    pr_enemy_gen me;
    int base_rc = 0;
    memset(&run, 0, sizeof run);
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "X")) { threw++; continue; }
        if (!strcmp(t, "P")) {
            cases++;
            memset(&run, 0, sizeof run);
            memset(&ctx, 0, sizeof ctx);
            memset(&me, 0, sizeof me);
            run.mode = (int)num(&save);
            pr_seed_from_ascii(&run.seed, tok(&save));
            char *ws = tok(&save);
            if (strcmp(ws, "-")) pr_seed_from_ascii(&run.wave_seed, ws);
            ctx.wave = (int)num(&save);
            ctx.biome = (int)num(&save);
            ctx.wave_cycle_offset = (int)num(&save);
            ctx.battle_type = (int)num(&save);
            ctx.trainer_type = (int)num(&save);
            ctx.trainer_slot = (int)num(&save);
            ctx.boss = num(&save) != 0;
            ctx.shiny_lock = num(&save) != 0;
            ctx.species = (int)num(&save);
            ctx.level = (int)num(&save);
            ctx.hidden_ability_charm_stacks = (int)num(&save);
            ctx.shiny_charm_stacks = (int)num(&save);
            ctx.enemy_fusion_stacks = (int)num(&save);
            ctx.enemy_fusion_chance = num(&save);
            ctx.trainer_id = (uint16_t)num(&save);
            ctx.secret_id = (uint16_t)num(&save);
            ctx.event_active = num(&save) != 0;
            ctx.event_shiny_multiplier = num(&save);
            ctx.event_trainer_shiny_chance = num(&save);
            read_state(&save, &run.rng);
            base_rc = pr_run_enemy_pokemon_base(&run, &ctx, &me);
        } else if (!strcmp(t, "M")) {
            if (base_rc) { uncovered[C_BASE]++; continue; }
            char *first = tok(&save);
            if (!strcmp(first, "none")) { record(C_BASE, false, line_no); base_rc = -1; continue; }
            bool ok = me.ability_index == atoi(first) && me.id == (uint32_t)num(&save);
            for (int i = 0; i < 6; i++) ok &= me.ivs[i] == (int)num(&save);
            ok &= me.gender == (int)num(&save);
            ok &= me.form == (int)num(&save);
            ok &= me.nature == (int)num(&save);
            ok &= me.tera_type == (int)num(&save);
            ok &= me.boss_segments == (int)num(&save);
            pr_rng after;
            read_state(&save, &after);
            ok &= same_state(&run.rng, &after);
            record(C_BASE, ok, line_no);
            if (!ok) base_rc = -1;   /* the second half starts from a wrong state */
        } else if (!strcmp(t, "F")) {
            if (base_rc) { uncovered[C_FINISH]++; continue; }
            unsigned before = run.unported;
            int rc = pr_run_enemy_pokemon_finish(&run, &ctx, &me);
            if (rc || run.unported != before) { uncovered[C_FINISH]++; continue; }
            num(&save);   /* fused: the base half already reported it */
            bool up_shiny = num(&save) != 0;
            int up_variant = (int)num(&save), up_luck = (int)num(&save), up_friend = (int)num(&save);
            bool ok = me.shiny == up_shiny && me.variant == up_variant && me.luck == up_luck && me.friendship == up_friend;
            for (int i = 0; i < 6; i++) ok &= me.ivs[i] == (int)num(&save);
            pr_rng after;
            read_state(&save, &after);
            ok &= same_state(&run.rng, &after);
            record(C_FINISH, ok, line_no);
            shiny += up_shiny;
            variants += up_variant > 0;
            bosses += ctx.boss;
            trainer_mons += ctx.trainer_slot != 0;
        }
    }
    printf("cases %d (addEnemyPokemon threw upstream: %d); checked: %d shiny, %d rare/epic variants, %d bosses, "
           "%d trainer Pokemon\n", cases, threw, shiny, variants, bosses, trainer_mons);
    int bad = 0;
    for (int c = 0; c < C_COUNT_; c++) {
        printf("  %-48s pass %6d  fail %d  not covered %d\n", check_names[c], pass[c], fail[c], uncovered[c]);
        bad |= fail[c];
    }
    return bad ? 1 : 0;
}
