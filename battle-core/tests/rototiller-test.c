/* Strict replay of the actual pinned Rototiller condition observations. */
#include "pr/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void bad(void) { fputs("invalid Rototiller fixture\n", stderr); exit(2); }

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    FILE *in = fopen(argv[1], "r");
    if (!in) return 2;
    char word[32];
    int version, count;
    if (fscanf(in, "%31s %d %d", word, &version, &count) != 3
        || strcmp(word, "ROTOTILLER") || version != 1 || count != 7) bad();
    pr_closures_init();
    const pr_move *move = &pr_moves[MOVE_ID_ROTOTILLER];
    if (move->conditions.n != 1) bad();
    int failures = 0;
    for (int c = 0; c < count; c++) {
        char label[32];
        int np, ne, species[12], alive[12], expected;
        if (fscanf(in, "%31s %31s %d %d", word, label, &np, &ne) != 4 || strcmp(word, "CASE")
            || np < 0 || np > 6 || ne < 0 || ne > 6) bad();
        for (int i = 0; i < np + ne; i++)
            if (fscanf(in, "%d", &species[i]) != 1 || species[i] <= 0 || species[i] >= PR_SPECIES_COUNT) bad();
        for (int i = 0; i < np + ne; i++)
            if (fscanf(in, "%d", &alive[i]) != 1 || (alive[i] != 0 && alive[i] != 1)) bad();
        if (fscanf(in, "%d", &expected) != 1 || (expected != 0 && expected != 1)) bad();

        static pr_battle bt;
        static pr_pokemon mons[12];
        memset(&bt, 0, sizeof bt);
        memset(mons, 0, sizeof mons);
        bt.double_battle = true;
        bt.n_party[0] = (uint8_t)np;
        bt.n_party[1] = (uint8_t)ne;
        for (int i = 0; i < np + ne; i++) {
            mons[i].species = (uint16_t)species[i];
            mons[i].hp = alive[i] ? 10 : 0;
            mons[i].id = (uint32_t)(i + 1);
            mons[i].is_player = i < np;
            bt.party[i < np ? 0 : 1][i < np ? i : i - np] = &mons[i];
        }
        for (int side = 0; side < 2; side++) {
            const int n = bt.n_party[side];
            for (int slot = 0; slot < 2; slot++) {
                const int index = side == 0 ? slot : np + slot;
                if (slot < n) {
                    bt.field[side * 2 + slot] = &mons[index];
                    mons[index].on_field = true;
                }
            }
        }
        const double actual = pr_closure_call(&bt, move->conditions.v[0], PR_AC_StatStageChangeAttr,
                                               0, &mons[0], &mons[np], MOVE_ID_ROTOTILLER);
        if ((int)actual != expected || bt.n_unported) {
            fprintf(stderr, "case %d: got %.0f expected %d unported=%u\n", c, actual, expected, bt.n_unported);
            failures++;
        }
    }
    int complete;
    if (fscanf(in, "%31s %d", word, &complete) != 2 || strcmp(word, "ROTOTILLER_COMPLETE") || complete != count) bad();
    if (fscanf(in, "%31s", word) == 1) bad();
    fclose(in);
    printf("Rototiller pinned-source party condition cases: %d, %d mismatches\n", count, failures);
    return failures ? 1 : 0;
}
