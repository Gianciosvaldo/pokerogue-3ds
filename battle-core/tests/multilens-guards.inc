/* Ownership/refusal/count guards; the focused oracle validates actual turns. */
static int multilens_guard_checks(void)
{
    int checks = 0;
    for (int side = 0; side < 2; side++) for (int mode = 0; mode < 9; mode++) {
        pr_battle bt = {0}; pr_pokemon p = {0};
        p.id = 10; p.is_player = !side;
        bt.n_mods[side] = 1;
        pr_modifier *m = &bt.mods[side][0];
        *m = (pr_modifier){.cls = PR_MOD_MULTI_HIT, .pokemon_id = p.id, .stack_count = 2};
        if (mode == 0) m->pokemon_id++;
        if (mode == 2) m->unknown_subclass = true;
        if (mode == 3) m->pokemon_id = -1;
        if (mode == 4) { m->stack_count = 1; m->virtual_stack_count = 1; }
        if (mode == 5) { bt.mods[!side][0] = *m; bt.n_mods[!side] = 1; bt.n_mods[side] = 0; }
        int move = mode == 6 ? MOVE_ID_DOUBLE_KICK : mode == 7 ? MOVE_ID_FLY : mode == 8 ? MOVE_ID_ENDEAVOR : MOVE_ID_TACKLE;
        pr_modifier original = *m; pr_rng rng = bt.rng;
        int count = 1;
        pr_apply_multi_hit_count_modifiers(&bt, mode == 1 ? NULL : &p, move, &count);
        if (count != ((mode == 3 || mode == 4) ? 3 : 1)) return 1;
        if (memcmp(&original, m, sizeof original) || memcmp(&rng, &bt.rng, sizeof rng)) return 1;
        if (mode == 2 && !bt.n_unported) return 1;
        pr_apply_multi_hit_count_modifiers(&bt, &p, move, NULL);
        checks++;
    }
    printf("Multi Lens guards: %d checks pass\n", checks);
    return 0;
}
