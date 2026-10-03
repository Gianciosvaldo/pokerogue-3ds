#define _POSIX_C_SOURCE 200809L
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
static long num(char **save) { return strtol(tok(save), NULL, 10); }

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "gym-e4-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }

    char line[4096];
    int line_no = 0, cases = 0, fights = 0, members = 0, fail = 0;
    pr_run run;
    pr_new_battle nb;
    pr_new_battle_ctx nc;
    pr_trainer_party_ctx pc;
    pr_party_template tpl;
    pr_trainer_party party;
    int member = 0, expected_members = 0;
    bool case_ok = true;

    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;

        if (!strcmp(t, "C")) {
            cases++;
            memset(&run, 0, sizeof run);
            memset(&nc, 0, sizeof nc);
            memset(&pc, 0, sizeof pc);
            memset(&tpl, 0, sizeof tpl);

            const bool fixed = num(&save) != 0;
            run.mode = GAME_MODES_CLASSIC;
            pr_seed_from_ascii(&run.seed, tok(&save));
            const int wave = (int)num(&save);
            const int biome = (int)num(&save);
            run.offset_gym = num(&save) != 0;
            const int wave_cycle_offset = (int)num(&save);
            const uint16_t trainer_id = (uint16_t)num(&save);
            const uint16_t secret_id = (uint16_t)num(&save);
            const int exp_type = (int)num(&save);
            const int exp_variant = (int)num(&save);
            const bool exp_double = num(&save) != 0;

            tpl.size = (int8_t)num(&save);
            tpl.evo_kind = (int8_t)num(&save);
            for (int i = 0; i < 8; i++) tpl.member[i] = (uint8_t)num(&save);
            for (int i = 0; i < 8; i++) tpl.offsets[i] = (int8_t)num(&save);
            const int nlevels = (int)num(&save);
            int exp_levels[PR_MAX_PARTY_TEMPLATE];
            for (int i = 0; i < nlevels; i++) exp_levels[i] = (int)num(&save);

            nc.biome = biome;
            nc.trainer_chance = 32;
            nc.prev_wave = wave - 1;
            nc.me.spawn_chance = 3;
            int rc = pr_run_new_battle(&run, wave, &nc, &nb);
            case_ok = rc == 0 && nb.battle_type == BATTLE_TYPE_TRAINER
                && nb.trainer_type == exp_type && nb.variant == exp_variant
                && nb.double_battle == exp_double && nb.n_levels == nlevels;
            for (int i = 0; case_ok && i < nlevels; i++) case_ok = nb.levels[i] == exp_levels[i];

            if (!case_ok) {
                if (++fail <= 20)
                    printf("MISMATCH line %d: %s wave %d trainer got %d expected %d rc=%d\n",
                           line_no, fixed ? "E4" : "gym", wave, nb.trainer_type, exp_type, rc);
            }

            pc.enemy.wave = wave;
            pc.enemy.biome = biome;
            pc.enemy.wave_cycle_offset = wave_cycle_offset;
            pc.enemy.battle_type = BATTLE_TYPE_TRAINER;
            pc.enemy.trainer_type = exp_type;
            pc.enemy.trainer_id = trainer_id;
            pc.enemy.secret_id = secret_id;
            pc.is_double = exp_double;
            pc.classic_wave20_trainer = wave == 20;
            pc.template_ = &tpl;
            pc.n_levels = nlevels;
            for (int i = 0; i < nlevels; i++) pc.levels[i] = exp_levels[i];

            if (case_ok && pr_run_gen_trainer_party(&run, &pc, &party) < 0) {
                case_ok = false;
                if (++fail <= 20) printf("MISMATCH line %d: trainer party flagged unported\n", line_no);
            }
            member = 0;
            expected_members = nlevels;
        } else if (!strcmp(t, "M")) {
            const int e = member++;
            const int species = atoi(tok(&save));
            char *snap = tok(&save);
            if (!case_ok || e >= party.n || !strcmp(snap, "none")) continue;
            const pr_enemy_gen *m = &party.mons[e];
            bool ok = party.species[e] == species && m->ability_index == atoi(snap);
            if (ok) {
                ok = m->id == (uint32_t)num(&save);
                ok = ok && m->gender == (int)num(&save);
                ok = ok && m->form == (int)num(&save);
                ok = ok && m->nature == (int)num(&save);
                ok = ok && m->tera_type == (int)num(&save);
                const int fs = (int)num(&save), fa = (int)num(&save), fg = (int)num(&save), ff = (int)num(&save);
                ok = ok && m->fusion_species == fs
                    && (fs < 0 || (m->fusion_ability_index == fa && m->fusion_gender == fg && m->fusion_form == ff));
            }
            if (!ok) {
                case_ok = false;
                if (++fail <= 20)
                    printf("MISMATCH line %d: member %d species got %d expected %d\n",
                           line_no, e, e < party.n ? party.species[e] : -1, species);
            } else {
                members++;
            }
            if (member == expected_members && case_ok) fights++;
        }
    }
    fclose(f);

    printf("gym/e4 summary: %d configurations, %d fully matched, %d matched members, %d mismatches\n",
           cases, fights, members, fail);
    return fail ? 1 : 0;
}
