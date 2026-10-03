/* Stuff Cheeks restriction: upstream@8555c08 src/data/moves/move.ts uses
 * findModifiers(m => m instanceof BerryModifier, user.isPlayer()).length === 0.
 * No owner, berry type or stack-count filter is present. Optional input records
 * are produced by berry.producer.test.ts using upstream's checkRestrictions. */
#include "pr/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cases, failures;

static void check(int player, int mod_side, int berry_type, int owner, int stack,
                  int unknown, int expected, int expect_unported)
{
    pr_battle bt;
    pr_pokemon p;
    memset(&bt, 0, sizeof(bt));
    memset(&p, 0, sizeof(p));
    p.is_player = player;
    p.id = 10;
    if (mod_side >= 0) {
        bt.n_mods[mod_side] = 1;
        bt.mods[mod_side][0] = (pr_modifier){
            .cls = berry_type >= 0 ? PR_MOD_BERRY : PR_MOD_HELD_ITEM,
            .berry_type = berry_type, .pokemon_id = owner,
            .stack_count = stack, .unknown_subclass = unknown
        };
    }
    int actual = pr_ai_move_selectable(&bt, &p, MOVE_ID_STUFF_CHEEKS);
    int unported = 0;
    for (int i = 0; i < bt.n_unported; i++)
        unported |= bt.unported[i] == PR_AI_FEAT_RESTRICTION;
    cases++;
    if (actual != expected || unported != expect_unported) {
        fprintf(stderr, "FAIL case %d: selectable=%d expected=%d unported=%d expected=%d\n",
                cases, actual, expected, unported, expect_unported);
        failures++;
    }
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        FILE *f = fopen(argv[1], "r");
        if (!f) { perror(argv[1]); return 2; }
        int player, side, type, owner, stack, expected;
        while (fscanf(f, "%d %d %d %d %d %d", &player, &side, &type, &owner, &stack, &expected) == 6)
            check(player, side, type, owner, stack, 0, expected, 0);
        if (!feof(f) || !cases) { fprintf(stderr, "invalid or empty producer output\n"); fclose(f); return 2; }
        fclose(f);
    } else if (argc == 1) {
        /* Both sides, every berry type, user/teammate/unowned, zero/nonzero stack. */
        const int owners[] = {10, 20, -1};
        for (int player = 0; player <= 1; player++) {
            int side = player ? 0 : 1;
            check(player, -1, -1, 10, 1, 0, 0, 0);
            check(player, side, -1, 10, 1, 0, 0, 0);
            check(player, side, -1, 10, 1, 1, 1, 1);
            check(player, 1-side, -1, 10, 1, 1, 0, 0);
            for (int type = BERRY_TYPE_SITRUS; type <= BERRY_TYPE_LEPPA; type++)
                for (int i = 0; i < 3; i++)
                    for (int stack = 0; stack <= 1; stack++) {
                        check(player, side, type, owners[i], stack, 0, 1, 0);
                        check(player, 1-side, type, owners[i], stack, 0, 0, 0);
                        check(player, side, type, owners[i], stack, 1, 1, 0);
                    }
        }
    } else return 2;
    printf("Stuff Cheeks: %d cases, %d identical, %d mismatches\n", cases, cases-failures, failures);
    return failures ? 1 : 0;
}
