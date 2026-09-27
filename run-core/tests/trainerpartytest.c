#define _POSIX_C_SOURCE 200809L
/* C half of run-core's trainer party test: replays the cases written by trainerparty.producer.test.ts and
 * compares, member by member, genPartyMember's species and the member's rolls up to its moveset. */
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
    FILE *f = fopen(argc > 1 ? argv[1] : "trainerparty-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }
    static char line[4096];
    int line_no = 0, cases = 0, pass = 0, fail = 0, uncovered = 0, threw = 0, parties = 0, doubles = 0;
    pr_run run;
    pr_trainer_party_ctx ctx;
    pr_party_template tpl;
    pr_trainer_party party;
    int member = 0, rc = 0;
    bool party_ok = true;
    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;
        if (!strcmp(t, "T")) {
            if (cases && party_ok && rc == 0) parties++;
            cases++;
            memset(&run, 0, sizeof run);
            memset(&ctx, 0, sizeof ctx);
            memset(&tpl, 0, sizeof tpl);
            run.mode = (int)num(&save);
            pr_seed_from_ascii(&run.seed, tok(&save));
            ctx.enemy.wave = (int)num(&save);
            ctx.enemy.biome = (int)num(&save);
            ctx.enemy.wave_cycle_offset = (int)num(&save);
            ctx.enemy.trainer_type = (int)num(&save);
            ctx.enemy.battle_type = BATTLE_TYPE_TRAINER;
            ctx.is_double = num(&save) != 0;
            ctx.classic_wave20_trainer = num(&save) != 0;
            ctx.enemy.trainer_id = (uint16_t)num(&save);
            ctx.enemy.secret_id = (uint16_t)num(&save);
            tpl.size = (int8_t)num(&save);
            tpl.evo_kind = (int8_t)num(&save);
            for (int i = 0; i < 8; i++) tpl.member[i] = (uint8_t)num(&save);
            for (int i = 0; i < 8; i++) tpl.offsets[i] = (int8_t)num(&save);
            ctx.template_ = &tpl;
            ctx.n_levels = (int)num(&save);
            for (int i = 0; i < ctx.n_levels && i < PR_MAX_PARTY_TEMPLATE; i++) ctx.levels[i] = (int)num(&save);
            doubles += ctx.is_double;
            rc = pr_run_gen_trainer_party(&run, &ctx, &party);
            member = 0;
            party_ok = true;
        } else if (!strcmp(t, "M")) {
            const int e = member++;
            char *first = tok(&save);
            if (!strcmp(first, "threw")) {
                threw++;
                party_ok = false;
                if (e < party.n && ++fail <= 10) printf("MISMATCH line %d: upstream threw, the port made member %d\n", line_no, e);
                continue;
            }
            if (e >= party.n) { uncovered++; party_ok = false; continue; }   /* past a flagged member */
            char *snap = tok(&save);
            const pr_enemy_gen *m = &party.mons[e];
            bool ok = party.species[e] == atoi(first) && strcmp(snap, "none") && m->ability_index == atoi(snap);
            if (ok) {
                ok = m->id == (uint32_t)num(&save);
                ok = ok && m->gender == (int)num(&save);
                ok = ok && m->form == (int)num(&save);
                ok = ok && m->nature == (int)num(&save);
                ok = ok && m->tera_type == (int)num(&save);
            }
            if (ok) pass++;
            else {
                party_ok = false;
                if (++fail <= 10) printf("MISMATCH line %d: member %d species %d\n", line_no, e, party.species[e]);
            }
        }
    }
    if (cases && party_ok && rc == 0) parties++;
    printf("cases %d (%d double; whole parties matched: %d; members upstream threw on: %d)\n", cases, doubles, parties,
           threw);
    printf("  %-36s pass %6d  fail %d  not covered %d\n", "genPartyMember (species, rolls)", pass, fail, uncovered);
    return fail ? 1 : 0;
}
