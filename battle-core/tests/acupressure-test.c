/* Strict comparison of actual native override queues and both RNG streams. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    input = fopen(argv[1], "r");
    if (!input) return 2;
    pr_closures_init();
    double h[2]; read_vector("ACUPRESSURE ", h, 2);
    if (h[0] != 1) bad();
    int n = integer(h[1], 16384, 100000), failures = 0, queued = 0;
    int attr = -1;
    for (int i = 0; i < pr_moves[MOVE_ID_ACUPRESSURE].nattr; i++) {
        int a = pr_moves[MOVE_ID_ACUPRESSURE].attr0 + i;
        if (pr_attrs[a].cls == PR_AC_AcupressureStatStageChangeAttr) attr = a;
    }
    if (attr < 0) return 2;
    for (int i = 0; i < n; i++) {
        static pr_battle bt; static pr_engine e;
        memset(&bt, 0, sizeof bt); memset(&e, 0, sizeof e);
        bt.engine = &e; bt.double_battle = true; e.current = -1; e.n_levels = 1;
        pr_pokemon mons[4] = {0};
        double c[4]; read_vector("C ", c, 4);
        if (c[0] != i) bad();
        int user = integer(c[1], 0, 3), target = integer(c[2], 0, 3), move = integer(c[3], 0, PR_MOVE_COUNT - 1);
        for (int k = 0; k < 4; k++) {
            double m[8]; read_vector("MON ", m, 8);
            mons[k].hp = integer(m[0], 0, INT_MAX); mons[k].id = k + 1;
            mons[k].species = SPECIES_ID_BULBASAUR; mons[k].on_field = true; mons[k].is_player = k < 2;
            for (int j = 0; j < 7; j++) mons[k].stat_stages[j] = integer(m[j+1], -6, 6);
            bt.field[k] = &mons[k]; bt.party[k/2][k%2] = &mons[k];
        }
        bt.n_party[0] = bt.n_party[1] = 2;
        rng_read("R ", &bt.rng); rng_read("B ", &bt.battle_rng);
        bt.rng_live = bt.battle_rng_live = true;
        int ph = pr_phase_new(&e, PH_MOVE_EFFECT, user);
        e.pool[ph].move_id = move;
        pr_apply_move_effect(&bt, &e, ph, attr, &mons[user], &mons[target]);
        bool exact = !bt.n_unported && !e.overflow;
        double v[1]; read_vector("RESULT ", v, 1);
        int result = integer(v[0], 0, 1), total = 0;
        read_vector("LEVELS ", v, 1); int nl = integer(v[0], 1, PR_MAX_LEVELS);
        exact &= nl == e.n_levels;
        for (int l = 0; l < nl; l++) {
            double q[2]; read_vector("L ", q, 2); if (q[0] != l) bad();
            int count = integer(q[1], 0, 1); total += count;
            exact &= l < e.n_levels && e.level_len[l] == count;
            for (int j = 0; j < count; j++) {
                if (strcmp(line_read("MARKER "), "StatStageChangePhase\n")) bad();
                if (l >= e.n_levels || j >= e.level_len[l]) { exact = false; continue; }
                const pr_phase *got = &e.pool[e.levels[l][j]];
                exact &= got->type == PH_MARKER && got->marker_of == PH_STAT_STAGE_CHANGE;
            }
        }
        if (total != result) bad();
        queued += total;
        read_vector("DYNAMIC ", v, 1); int nd = integer(v[0], 0, 1);
        if (nd != result) bad();
        for (int k = 0; k < PH_COUNT_; k++) exact &= e.n_dyn[k] == (k == PH_STAT_STAGE_CHANGE ? nd : 0);
        for (int j = 0; j < nd; j++) {
            double p[5]; read_vector("PH ", p, 5);
            int b = integer(p[0], 0, 3), src = integer(p[1], 0, 3), stat = integer(p[2], STAT_ATK, STAT_EVA);
            int stages = integer(p[3], -12, 12), self = integer(p[4], 0, 1);
            if (j >= e.n_dyn[PH_STAT_STAGE_CHANGE]) { exact = false; continue; }
            const pr_phase *got = &e.pool[e.dyn[PH_STAT_STAGE_CHANGE][j]];
            exact &= got->type == PH_STAT_STAGE_CHANGE && got->battler == b && got->source_p == &mons[src]
                && got->n_ssc == 1 && got->ssc_stat[0] == stat && got->ssc_stages[0] == stages
                && got->ssc_self_target == self && !got->ssc_processed && !got->ssc_ignore_abilities
                && !got->ssc_message && !got->ssc_source_type;
        }
        for (int k = 0; k < 4; k++) {
            double m[8]; read_vector("AFTER ", m, 8);
            exact &= mons[k].hp == integer(m[0], 0, INT_MAX);
            for (int j = 0; j < 7; j++) exact &= mons[k].stat_stages[j] == integer(m[j+1], -6, 6);
        }
        pr_rng rng; rng_read("RO ", &rng); exact &= rng_equal(bt.rng, rng);
        rng_read("BO ", &rng); exact &= rng_equal(bt.battle_rng, rng);
        if (!exact) { if (failures < 12) fprintf(stderr, "Acupressure case %d failed\n", i); failures++; }
    }
    double footer[1]; read_vector("MELOETTA ", footer, 1);
    if (footer[0] != 40) bad();
    int noops = 0, boundaries = 0;
    for (int i = 0; i < 40; i++) {
        double m[7]; read_vector("MNOOP ", m, 7); if (m[0] != i) bad();
        int side = integer(m[1], 0, 1), form = integer(m[2], 0, 1), hp = integer(m[3], 0, 1);
        int move = integer(m[4], -1, PR_MOVE_COUNT-1), result = integer(m[5], 0, 1), count = integer(m[6], 0, 1);
        if (result != count) bad();
        static pr_battle bt; static pr_engine e;
        memset(&bt, 0, sizeof bt); memset(&e, 0, sizeof e);
        bt.engine = &e; e.current = -1; e.n_levels = 1;
        pr_pokemon p = {.species=SPECIES_ID_MELOETTA, .form=form, .hp=hp, .on_field=true, .is_player=!side};
        bt.field[side*2] = &p;
        if (move >= 0) pr_push_move_history(&p, (pr_turn_move){.move=move});
        pr_form_change(&bt, &p, PR_FCT_SpeciesFormChangePostMoveTrigger);
        bool exact = !e.overflow;
        for (int k = 0; k < e.n_levels; k++) exact &= e.level_len[k] == 0;
        for (int k = 0; k < PH_COUNT_; k++) exact &= e.n_dyn[k] == 0;
        if (!count) { noops++; exact &= bt.n_unported == 0; }
        else { boundaries++; exact &= bt.n_unported == 1 && bt.unported[0] == PR_FEAT_FORM_CHANGE; }
        if (!exact) failures++;
    }
    read_vector("ACUPRESSURE_COMPLETE ", footer, 1);
    if (footer[0] != n || fgetc(input) != EOF) bad();
    printf("Acupressure: %d complete API cases, %d queued phases, %d failures\n", n, queued, failures);
    printf("Meloetta: %d native no-op predicates matched; %d positive transitions remain unsupported\n", noops, boundaries);
    return failures ? 1 : 0;
}
