/* Strict replay of the pinned Healing Wish/Lunar Dance condition closures. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void bad(void) { fputs("invalid Healing Wish party fixture\n", stderr); exit(2); }

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *in = fopen(argv[1], "r");
    if (!in) return 2;
    char word[32], label[64];
    int version, count;
    if (fscanf(in, "%31s %d %d", word, &version, &count) != 3
        || strcmp(word, "HEALING_WISH_PARTY") || version != 1 || count != 6) bad();
    pr_closures_init();
    int failures = 0;
    for (int c = 0; c < count; c++) {
        int move_id, user_side, np, ne, active[12], on_field[12], ncond, expected[2], allowed;
        if (fscanf(in, "%31s %63s %d %d %d %d", word, label, &move_id, &user_side, &np, &ne) != 6
            || strcmp(word, "CASE") || (move_id != MOVE_ID_HEALING_WISH && move_id != MOVE_ID_LUNAR_DANCE)
            || (user_side != 0 && user_side != 1) || np < 1 || np > 6 || ne < 1 || ne > 6) bad();
        for (int i = 0; i < np + ne; i++) {
            if (fscanf(in, "%d %d", &active[i], &on_field[i]) != 2
                || (active[i] != 0 && active[i] != 1) || (on_field[i] != 0 && on_field[i] != 1)) bad();
        }
        if (fscanf(in, "%d", &ncond) != 1 || ncond != 2
            || fscanf(in, "%d %d %d", &expected[0], &expected[1], &allowed) != 3
            || (expected[0] != 0 && expected[0] != 1) || (expected[1] != 0 && expected[1] != 1)
            || (allowed != 0 && allowed != 1)) bad();

        static pr_battle bt;
        static pr_pokemon mons[12];
        memset(&bt, 0, sizeof bt);
        memset(mons, 0, sizeof mons);
        bt.double_battle = true;
        bt.n_party[0] = (uint8_t)np;
        bt.n_party[1] = (uint8_t)ne;
        for (int i = 0; i < np + ne; i++) {
            mons[i].species = SPECIES_ID_RATTATA;
            mons[i].hp = active[i] ? 10 : 0;
            mons[i].id = (uint32_t)(i + 1);
            mons[i].is_player = i < np;
            mons[i].on_field = on_field[i];
            bt.party[i < np ? 0 : 1][i < np ? i : i - np] = &mons[i];
            if (on_field[i]) {
                const int side = i < np ? 0 : 1;
                int slot = 0;
                while (slot < 2 && bt.field[side * 2 + slot]) slot++;
                if (slot == 2) bad();
                bt.field[side * 2 + slot] = &mons[i];
            }
        }
        const pr_move *move = &pr_moves[move_id];
        if (move->conditions.n != 2) bad();
        const pr_pokemon *user = user_side == 0 ? &mons[0] : &mons[np];
        const pr_pokemon *target = user_side == 0 ? &mons[np] : &mons[0];
        int mismatch = 0, native_results[2];
        for (int k = 0; k < 2; k++) {
            const double actual = pr_closure_call(&bt, move->conditions.v[k], PR_AC_StatStageChangeAttr,
                                                  0, user, target, move_id);
            native_results[k] = (int)actual;
            mismatch |= actual != expected[k];
        }
        const bool actual_allowed = native_results[0] && native_results[1];
        if (mismatch || actual_allowed != (bool)allowed || bt.n_unported) {
            fprintf(stderr, "case %d (%s): mismatch=%d allowed=%d/%d unported=%u\n",
                    c, label, mismatch, actual_allowed, allowed, bt.n_unported);
            failures++;
        }
    }
    int complete;
    if (fscanf(in, "%31s %d", word, &complete) != 2
        || strcmp(word, "HEALING_WISH_PARTY_COMPLETE") || complete != count) bad();
    if (fscanf(in, "%31s", word) == 1) bad();
    fclose(in);
    printf("Healing Wish/Lunar Dance pinned condition cases: %d, %d mismatches\n", count, failures);
    return failures ? 1 : 0;
}
