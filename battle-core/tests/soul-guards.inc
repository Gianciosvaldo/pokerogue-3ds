/* Existing arithmetic is unchanged; unsupported identities refuse before mutation. */
static int soul_guard_checks(void)
{
    int checks = 0;
    for (int mode = 0; mode < 16; mode++) {
        pr_battle bt = {0}; pr_pokemon p = {0};
        p.id = 10; p.is_player = mode != 9;
        int side = p.is_player ? 0 : 1;
        bt.n_mods[side] = 1;
        pr_modifier *m = &bt.mods[side][0];
        *m = (pr_modifier){.cls = PR_MOD_NATURE_WEIGHT, .pokemon_id = p.id, .stack_count = 1};
        double value = mode == 5 ? 1 : mode == 7 || mode == 8 ? 0.9 : 1.1;
        double want = value;
        if (mode == 0) m->pokemon_id++;
        if (mode == 1) { bt.mods[1][0] = *m; bt.n_mods[1] = 1; bt.n_mods[0] = 0; }
        if (mode == 2) m->unknown_subclass = true;
        if (mode == 3) m->stack_count = 11;
        if (mode == 4) m->virtual_stack_count = 10;
        if (mode == 6) m->pokemon_id = -1;
        if (mode == 7) m->virtual_stack_count = 2;
        if (mode == 8) m->stack_count = 10;
        if (mode == 10) {
            bt.n_mods[side] = 2; bt.mods[side][1] = *m; bt.mods[side][1].stack_count = 10;
        }
        if (mode == 14) m->stack_count = 0;
        if (mode == 15) m->virtual_stack_count = -1;
        if (mode == 6 || mode == 9 || mode == 10 || mode == 13) want = value + 0.1;
        if (mode == 7) want = value - 0.1 * 3;
        if (mode == 8) want = value - 0.1 * 10;
        pr_modifier original[2][PR_MAX_MODIFIERS]; memcpy(original, bt.mods, sizeof original);
        pr_rng g = bt.rng, b = bt.battle_rng;
        pr_apply_nature_weight_modifier(&bt, mode == 11 ? NULL : &p, mode == 12 ? NULL : &value);
        if (value != want || memcmp(original, bt.mods, sizeof original)
            || memcmp(&g, &bt.rng, sizeof g) || memcmp(&b, &bt.battle_rng, sizeof b)) return 1;
        if ((mode == 2 || mode == 3 || mode == 4 || mode == 14 || mode == 15) && !bt.n_unported) return 1;
        checks++;
    }
    printf("Soul Dew guards: %d checks pass\n", checks);
    return 0;
}
