/* Gym leaders, the Elite Four, the Champion and the evil teams' fixed fights in the run (src/gc_encounter.c: gc_new_battle and gen_trainer_party), against the pinned upstream.
 *
 * The cases are the ones run-core's gym_e4 producer writes (run-core/tests/gym_e4.producer.test.ts, left as it is): for a random run seed, wave cycle
 * offset, offsetGym, biome and trainer/secret ids, the real gym leader of a boss wave (the biome's boss pool) or the real fixed Elite Four / Champion
 * of waves 182..190, with the party template, the levels and every member's species and constructor state. Here the same inputs go to gc_new_battle,
 * the way the app starts the wave, and what the run ends up holding is compared: that the battle is the trainer battle (not the wild stand-in
 * game-core used before it switched run-core's Gym/E4 closures on), the trainer type and variant, double or single, the number of members and each
 * one's level, species, id, gender, form, nature, ability slot, Tera type and fusion.
 *
 * The same comparator replays tools/evildiff.sh's trace (game-core/tests/evil.producer.test.ts): the Town Youngster and the evil team grunt fights
 * (and, once ported, the admins and bosses). Upstream rolls a grunt's gender and double variant with Math.random, so the producer writes them in an
 * "R <gender roll> <double roll>" line before the case and they are pinned in the run (evil_roll_pinned) instead of drawn.
 *
 * usage: gym_e4_diff_test <cases file> */
#define _POSIX_C_SOURCE 200809L
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int roll_gender, roll_double;
static bool case_ok = true, case_admin_or_boss;
static int mismatches, members, school_forms, post_checked;

static char *tok(char **save)
{
    char *t = strtok_r(NULL, " \n", save);
    if (!t) { fprintf(stderr, "short line\n"); exit(2); }
    return t;
}
static long num(char **save) { return strtol(tok(save), NULL, 10); }

/* a run at the end of wave - 1 with a party of three, so that gc_new_battle starts `wave` */
static void start(const char *seed, int wave, int biome, bool offset_gym, int wave_cycle_offset, int trainer_id, int secret_id)
{
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_system_init_defaults(&sys);
    run.sys = &sys;
    pr_seed_from_ascii(&run.run.seed, seed);
    run.run.offset_gym = offset_gym;
    run.wave_cycle_offset = wave_cycle_offset;
    run.trainer_id = trainer_id;
    run.secret_id = secret_id;
    run.biome = biome;
    run.n_items = 0;
    run.n_party = 3;
    for (int i = 0; i < 3; i++) {
        gc_starter s;
        memset(&s, 0, sizeof s);
        s.species = (uint16_t)(i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_GENGAR);
        s.tera_type = -1;
        for (int k = 0; k < 6; k++) s.ivs[k] = 15;
        s.moves[0] = MOVE_ID_TACKLE;
        gc_mon_new_player(&run, &run.party[i], &s, 100);
        run.party_order[i] = (int8_t)i;
    }
    run.battle_type = BATTLE_TYPE_WILD;
    gc_items_to_battle(&run);
    gc_rq_reset(&gc_int(&run)->rq);
    run.wave = wave - 1;
    run.in_battle = true;
}

/* the last member's constructor snapshot, until its P line (or the next member) says what the post-process left: its ability slot, gender and form
 * are compared with the P line's when there is one, with the snapshot's when there is not (the gym leaders' producer writes no P lines) */
static struct { bool open, fixed; int line, e, species, ability, gender, form; } pend;

static void mismatch(int line, bool fixed, int e, int species, const char *what, int got, int expected)
{
    case_ok = false;
    if (++mismatches <= 20)
        printf("MISMATCH line %d: %s member %d (species %d): %s differs (got %d expected %d)\n", line, fixed ? "fixed" : "gym", e, species, what, got, expected);
}

static void flush_pending(void)
{
    if (!pend.open) return;
    pend.open = false;
    if (!case_ok || pend.e >= run.n_enemy) return;
    const gc_mon *m = &run.enemy[pend.e];
    /* the producer's snapshot is taken before the member is summoned: Wishiwashi of level 20 or more (healthy) is in its School form
     * (Schooling, PostSummonFormChangeAbAttr) once the battle starts, which game-core has applied by the end of gc_new_battle */
    const bool schooling = pend.species == SPECIES_ID_WISHIWASHI && pend.form == 0 && m->b.form == 1 && m->b.level >= 20;
    if (schooling) school_forms++;
    if (m->ability_index != pend.ability) mismatch(pend.line, pend.fixed, pend.e, pend.species, "ability slot", m->ability_index, pend.ability);
    else if (m->b.gender != pend.gender) mismatch(pend.line, pend.fixed, pend.e, pend.species, "gender", m->b.gender, pend.gender);
    else if (m->b.form != pend.form && !schooling) mismatch(pend.line, pend.fixed, pend.e, pend.species, "form", m->b.form, pend.form);
    else members++;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "gym-e4-cases.txt", "r");
    if (!f) { perror("cases"); return 2; }

    char line[4096];
    int line_no = 0, cases = 0, matched = 0, gyms = 0, fixed_fights = 0, stand_ins = 0, no_items = 0;
    int member = 0, expected_members = 0;
    bool case_fixed = false;

    while (fgets(line, sizeof line, f)) {
        line_no++;
        char *save;
        char *t = strtok_r(line, " \n", &save);
        if (!t || t[0] == '#') continue;

        if (strcmp(t, "P")) flush_pending();
        if (!strcmp(t, "R")) {
            roll_gender = (int)num(&save);
            roll_double = (int)num(&save);
        } else if (!strcmp(t, "T")) {
            /* the trainer's name and its partner's: upstream's locale keys end in the 1-based entry run-core rolled (index + 1); "-" = none */
            const char *nk = tok(&save), *pk = tok(&save);
            const char *dot = strrchr(nk, '.'), *pdot = strrchr(pk, '.');
            const int name = !strcmp(nk, "-") || !dot ? -1 : atoi(dot + 1) - 1;
            const int partner = !strcmp(pk, "-") || !pdot ? -1 : atoi(pdot + 1) - 1;
            if (case_ok && (run.trainer_name != name || run.trainer_partner_name != partner)) {
                case_ok = false;
                if (++mismatches <= 20)
                    printf("MISMATCH line %d: names got %d / %d expected %d / %d (%s %s)\n", line_no, run.trainer_name, run.trainer_partner_name, name, partner, nk, pk);
            }
        } else if (!strcmp(t, "C")) {
            if (cases && case_ok) matched++;
            cases++;
            const bool fixed = num(&save) != 0;
            const char *seed = tok(&save);
            const int wave = (int)num(&save);
            const int biome = (int)num(&save);
            const bool offset_gym = num(&save) != 0;
            const int wave_cycle_offset = (int)num(&save);
            const int trainer_id = (int)num(&save);
            const int secret_id = (int)num(&save);
            const int exp_type = (int)num(&save);
            const int exp_variant = (int)num(&save);
            const bool exp_double = num(&save) != 0;
            for (int i = 0; i < 2 + 8 + 8; i++) num(&save);   /* the party template: run-core's own comparator checks it */
            const int nlevels = (int)num(&save);
            int exp_levels[PR_MAX_PARTY_TEMPLATE];
            for (int i = 0; i < nlevels && i < PR_MAX_PARTY_TEMPLATE; i++) exp_levels[i] = (int)num(&save);

            start(seed, wave, biome, offset_gym, wave_cycle_offset, trainer_id, secret_id);
            gc_int(&run)->evil_roll_pinned = true;
            gc_int(&run)->post_sentinel_moves = true;   /* the producer's moveset stub: moves 1..4 */
            gc_int(&run)->evil_roll[0] = (uint8_t)roll_gender;
            gc_int(&run)->evil_roll[1] = (uint8_t)roll_double;
            gc_new_battle(&run);
            case_fixed = fixed;
            /* only the admins' and bosses' closures leave the producer's stub moves; an ordinary member's moveset is the generator's, not the stub */
            case_admin_or_boss = fixed && (wave == 66 || wave == 114 || wave == 164 || wave == 115 || wave == 165);
            if (fixed) fixed_fights++; else gyms++;

            case_ok = run.battle_type == BATTLE_TYPE_TRAINER && run.trainer_type == exp_type && run.trainer_variant == exp_variant
                && run.double_battle == exp_double && run.n_enemy == nlevels && run.wave == wave;
            /* none of the stand-ins game-core falls back to: a fixed battle or a trainer config run-core could not make. The one stand-in left is the
             * held items of the trainers whose config has a genModifiersFunc (Red, Steven, Wallace and Kieran among the Champions): run-core's item
             * generation reports PR_RUN_UNPORTED_TRAINER for them and they hold nothing; their teams are the real ones */
            const bool gen_modifiers = exp_type >= 0 && exp_type < pr_trainer_item_flag_count && (pr_trainer_item_flags[exp_type] & 2);
            if (run.stubs[GC_STUB_UNPORTED_ENEMY] != (gen_modifiers ? 1u : 0u) || run.run_core_unported[4] || run.run_core_unported[5]) {
                case_ok = false;
                stand_ins++;
            }
            if (gen_modifiers) no_items++;
            for (int i = 0; case_ok && i < nlevels; i++) case_ok = run.enemy[i].b.level == exp_levels[i];
            if (!case_ok && ++mismatches <= 20)
                printf("MISMATCH line %d: %s wave %d got type %d (battle %d) expected %d, %d members expected %d\n", line_no,
                       fixed ? "fixed" : "gym", wave, run.trainer_type, run.battle_type, exp_type, run.n_enemy, nlevels);
            member = 0;
            expected_members = nlevels;
        } else if (!strcmp(t, "M")) {
            const int e = member++;
            const int species = atoi(tok(&save));
            char *snap = tok(&save);
            if (!case_ok || e >= run.n_enemy || !strcmp(snap, "none")) continue;
            const gc_mon *m = &run.enemy[e];
            /* the snapshot: ability slot, id, gender, form, nature, Tera type, then the fusion */
            const int x_ability = atoi(snap);
            const uint32_t x_id = (uint32_t)num(&save);
            const int x_gender = (int)num(&save), x_form = (int)num(&save), x_nature = (int)num(&save), x_tera = (int)num(&save);
            const int fs = (int)num(&save), fa = (int)num(&save), fg = (int)num(&save), ff = (int)num(&save);
            const char *bad = NULL;
            if (m->b.species != species) bad = "species";
            else if (m->b.id != x_id) bad = "id";
            else if (m->b.nature != x_nature) bad = "nature";
            else if (m->b.tera_type != x_tera) bad = "Tera type";
            else if ((m->b.fusion_species ? m->b.fusion_species : -1) != fs) bad = "fusion species";
            else if (fs >= 0 && (m->b.fusion_ability_index != fa || m->b.fusion_gender != fg || m->b.fusion_form != ff)) bad = "fusion state";
            if (bad) {
                mismatch(line_no, case_fixed, e, species, bad, 0, 0);
            } else {
                pend.open = true;
                pend.fixed = case_fixed;
                pend.line = line_no;
                pend.e = e;
                pend.species = species;
                pend.ability = x_ability;
                pend.gender = x_gender;
                pend.form = x_form;
            }
        } else if (!strcmp(t, "P")) {
            /* what the post-process left: ability slot, gender, form, boss segments, friendship, the four moves */
            const int x_ability = (int)num(&save), x_gender = (int)num(&save), x_form = (int)num(&save), x_boss = (int)num(&save), x_friend = (int)num(&save);
            int x_moves[4];
            for (int k = 0; k < 4; k++) x_moves[k] = (int)num(&save);
            if (!pend.open) continue;
            pend.open = false;
            if (!case_ok || pend.e >= run.n_enemy) continue;
            const gc_mon *m = &run.enemy[pend.e];
            const bool schooling = pend.species == SPECIES_ID_WISHIWASHI && x_form == 0 && m->b.form == 1 && m->b.level >= 20;
            if (schooling) school_forms++;
            const int l = line_no, e = pend.e, sp = pend.species;
            bool moves_ok = true;
            for (int k = 0; k < 4; k++) moves_ok &= !case_admin_or_boss || (k < m->b.n_moves ? m->b.moves[k] : 0) == x_moves[k];
            post_checked++;
            if (m->ability_index != x_ability) mismatch(l, pend.fixed, e, sp, "ability slot after postProcess", m->ability_index, x_ability);
            else if (m->b.gender != x_gender) mismatch(l, pend.fixed, e, sp, "gender after postProcess", m->b.gender, x_gender);
            else if (m->b.form != x_form && !schooling) mismatch(l, pend.fixed, e, sp, "form after postProcess", m->b.form, x_form);
            else if (m->b.boss_segments != x_boss) mismatch(l, pend.fixed, e, sp, "boss segments", m->b.boss_segments, x_boss);
            else if (m->b.friendship != x_friend) mismatch(l, pend.fixed, e, sp, "friendship", m->b.friendship, x_friend);
            else if (!moves_ok) mismatch(l, pend.fixed, e, sp, "moves", m->b.moves[0], x_moves[0]);
            else members++;
        }
    }
    flush_pending();
    if (cases && case_ok) matched++;
    fclose(f);
    (void)expected_members;

    printf("gym/e4/evil in game-core: %d cases (%d gym waves, %d fixed fights), %d fully matched, %d members, %d stand-ins, %d without held items "
           "(genModifiersFunc champions), %d Wishiwashi already in School form, %d mismatches\n",
           cases, gyms, fixed_fights, matched, members, stand_ins, no_items, school_forms, mismatches);
    printf("%d members compared after their postProcess (ability slot, gender, form, boss segments, friendship, moves)\n", post_checked);
    printf("TOTAL %d cases, %d mismatches\n", cases, mismatches);
    return mismatches || !cases ? 1 : 0;
}
