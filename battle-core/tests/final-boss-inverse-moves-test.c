/* Final-boss phase two applies the Inverse Battle move override from upstream. */
#include "pr/engine.h"
#include <stdio.h>

static int check(int mode, bool flags_known, uint32_t flags, uint16_t third_move, int unported)
{
    pr_battle bt = {0};
    pr_engine engine = {0};
    pr_pokemon boss = {0};
    bt.engine = &engine;
    bt.game_mode = mode;
    bt.wave_index = 200;
    bt.challenge_flags_known = flags_known;
    bt.challenge_flags = flags;
    engine.n_levels = 1;
    engine.current = -1;
    boss.id = 9;
    boss.species = SPECIES_ID_ETERNATUS;
    boss.is_boss = true;
    boss.on_field = true;
    boss.boss_segments = 4;
    bt.n_party[1] = 1;
    bt.party[1][0] = bt.field[2] = &boss;

    pr_init_final_boss_phase_two(&bt, &engine, &boss);
    bool good = boss.n_moves == 4
        && boss.moves[0] == MOVE_ID_DYNAMAX_CANNON
        && boss.moves[1] == MOVE_ID_CROSS_POISON
        && boss.moves[2] == third_move
        && boss.moves[3] == MOVE_ID_RECOVER
        && boss.pp_up[3] == -4
        && bt.n_unported == unported;
    if (!good) {
        fprintf(stderr, "mode%d known%d flags%u: moves %u/%u/%u/%u ppup%d unported%d; expected third%u/unported%d\n",
                mode, flags_known, flags, boss.moves[0], boss.moves[1], boss.moves[2], boss.moves[3],
                boss.pp_up[3], bt.n_unported, third_move, unported);
    }
    return !good;
}

int main(void)
{
    int failed = 0;
    failed += check(GAME_MODES_CLASSIC, false, 0, MOVE_ID_FLAMETHROWER, 0);
    failed += check(GAME_MODES_CHALLENGE, true, 0, MOVE_ID_FLAMETHROWER, 0);
    failed += check(GAME_MODES_CHALLENGE, true, 1u << CHALLENGES_INVERSE_BATTLE, MOVE_ID_EARTH_POWER, 0);
    failed += check(GAME_MODES_CHALLENGE, false, 0, MOVE_ID_FLAMETHROWER, 1);
    printf("Final-boss phase-two move sets: 4 cases, %d mismatches\n", failed);
    return failed != 0;
}
