/* Strict replay of native Quash callback observations. */
#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main

static void message_read(char *out, size_t cap)
{
    char *s = line_read("MSG "); s[strcspn(s, "\n")] = 0;
    size_t n = strlen(s);
    if (!n || n % 2 || strspn(s, "0123456789abcdef") != n || n / 2 >= cap) bad();
    for (size_t i = 0; i < n / 2; i++) {
        unsigned b; if (sscanf(s + 2*i, "%2x", &b) != 1 || !b) bad(); out[i] = b;
    }
    out[n/2] = 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) return 2;
    input = fopen(argv[1], "r"); if (!input) return 2;
    pr_closures_init();
    double h[2]; read_vector("QUASH ", h, 2); if (h[0] != 1) bad();
    int n = integer(h[1], 8192, 100000), failures = 0, operations = 0, attr = -1;
    const pr_move *move = &pr_moves[MOVE_ID_QUASH];
    for (int j = 0; j < move->nattr; j++) if (pr_attrs[move->attr0+j].cls == PR_AC_ForceLastAttr) attr = move->attr0+j;
    if (attr < 0) return 2;
    for (int i = 0; i < n; i++) {
        static pr_battle bt; static pr_engine e; memset(&bt, 0, sizeof bt); memset(&e, 0, sizeof e);
        bt.engine = &e; bt.double_battle = true; e.current = -1; e.n_levels = 1;
        double c[6]; read_vector("C ", c, 6); if (c[0] != i) bad();
        int user = integer(c[1], 0, 3), target = integer(c[2], 0, 3), count = integer(c[3], 0, 8), ops = integer(c[4], 1, 3);
        int swap = integer(c[5], 0, 3);
        (void)target;
        pr_pokemon actors[4] = {0};
        for (int k = 0; k < 4; k++) {
            double m[4]; read_vector("MON ", m, 4);
            actors[k].species = integer(m[0], 1, 65535); actors[k].form = integer(m[1], 0, 255);
            const pr_species *sp = pr_pokemon_species(&actors[k]); if (!sp || actors[k].form >= sp->nforms) bad();
            actors[k].hp = integer(m[2], 0, INT_MAX); actors[k].is_player = k < 2; actors[k].on_field = true;
            actors[k].acted = integer(m[3], 0, 1);
            actors[k].id = k + 1; bt.field[k] = &actors[k]; bt.party[k/2][k%2] = &actors[k];
        }
        bt.n_party[0] = bt.n_party[1] = 2;
        for (int j = 0; j < count; j++) {
            double m[2]; read_vector("MOVE ", m, 2); int who = integer(m[0], 0, 3), timing = integer(m[1], 0, 2);
            int ph = pr_phase_new(&e, PH_MOVE, who); e.pool[ph].pokemon = &actors[who];
            e.pool[ph].timing = timing; e.pool[ph].move.id = MOVE_ID_TACKLE; pr_phase_push(&bt, &e, ph);
        }
        for (int side = 0; side < 2; side++) if (swap & (1 << side)) {
            bt.field[side*2] = bt.party[side][0] = &actors[side*2+1];
            bt.field[side*2+1] = bt.party[side][1] = &actors[side*2];
        }
        rng_read("R ", &bt.rng); rng_read("B ", &bt.battle_rng); bt.rng_live = bt.battle_rng_live = true;
        int effect = pr_phase_new(&e, PH_MOVE_EFFECT, user); e.pool[effect].move_id = MOVE_ID_QUASH;
        bool exact = true;
        for (int op = 0; op < ops; op++) {
            double o[2]; read_vector("OP ", o, 2); int to = integer(o[0], 0, 3); if (o[1] != 1) bad();
            operations++; pr_apply_move_effect(&bt, &e, effect, attr, &actors[user], &actors[to]);
            exact &= !bt.n_unported && !e.overflow;
            double v[1]; read_vector("LEVELS ", v, 1); int levels = integer(v[0], 1, PR_MAX_LEVELS); exact &= levels == e.n_levels;
            for (int l = 0; l < levels; l++) {
                double q[2]; read_vector("L ", q, 2); if (q[0] != l) bad(); int len = integer(q[1], 0, PR_LEVEL_CAP);
                exact &= l < e.n_levels && len == e.level_len[l];
                for (int j = 0; j < len; j++) {
                    int ch = fgetc(input); if (ch == EOF || ungetc(ch, input) == EOF) bad();
                    const pr_phase *p = l < e.n_levels && j < e.level_len[l] ? &e.pool[e.levels[l][j]] : NULL;
                    if (ch == 'M' && count && j < count && l == 0) {
                        if (strcmp(line_read("MARKER "), "MovePhase\n")) bad();
                        exact &= p && p->type == PH_MARKER && p->marker_of == PH_MOVE;
                    } else {
                        char expected[2048], actual[2048]; message_read(expected, sizeof expected);
                        if (!p || p->type != PH_MESSAGE) { exact = false; continue; }
                        pr_text_source text = {locale_get, NULL, "en"}; pr_msg_format(&p->msg, &text, actual, sizeof actual);
                        exact &= !strcmp(expected, actual);
                    }
                }
            }
            read_vector("DYNAMIC ", v, 1); int nd = integer(v[0], 0, 8); if (nd != count) bad();
            for (int k = 0; k < PH_COUNT_; k++) exact &= e.n_dyn[k] == (k == PH_MOVE ? nd : 0);
            for (int j = 0; j < nd; j++) {
                double d[2]; read_vector("D ", d, 2); int who = integer(d[0], 0, 3), timing = integer(d[1], 0, 2);
                if (j >= e.n_dyn[PH_MOVE]) { exact = false; continue; }
                const pr_phase *p = &e.pool[e.dyn[PH_MOVE][j]];
                exact &= p->type == PH_MOVE && p->pokemon == &actors[who] && p->timing == timing;
            }
        }
        pr_rng r; rng_read("RO ", &r); exact &= rng_equal(bt.rng, r); rng_read("BO ", &r); exact &= rng_equal(bt.battle_rng, r);
        for (int k = 0; k < 4; k++) {
            double v[2]; read_vector("AFTER ", v, 2);
            int hp = integer(v[0], 0, INT_MAX), acted = integer(v[1], 0, 1);
            exact &= actors[k].hp == hp && actors[k].acted == acted;
        }
        if (!exact) { if (failures < 12) fprintf(stderr, "Quash case%d failed\n", i); failures++; }
    }
    double footer[1]; read_vector("QUASH_COMPLETE ", footer, 1); if (footer[0] != n || fgetc(input) != EOF) bad();
    printf("Quash: %d complete API cases, %d operations, %d failures\n", n, operations, failures);
    return failures ? 1 : 0;
}
