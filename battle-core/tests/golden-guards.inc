/* Guard the state boundary: unsupported applicable dependency refuses atomically. */
static int golden_guard_checks(void)
{
    int checks = 0;
    for (int mode = 0; mode < 12; mode++) {
        pr_battle bt = {0}; pr_pokemon p = {0};
        p.id = 10; p.is_player = true; bt.money = 100;
        bt.n_mods[0] = 2;
        bt.mods[0][0] = (pr_modifier){.cls = PR_MOD_DAMAGE_MONEY_REWARD, .pokemon_id = p.id, .stack_count = 1};
        bt.mods[0][1] = (pr_modifier){.cls = PR_MOD_MONEY_MULTIPLIER, .stack_count = 1};
        if (mode == 0) bt.mods[0][0].pokemon_id++;
        if (mode == 1) p.is_player = false;
        if (mode == 2) bt.mods[0][0].unknown_subclass = true;
        if (mode == 3) bt.mods[0][1].unknown_subclass = true;
        if (mode == 4) bt.mods[0][0].stack_count = 6;
        if (mode == 5) bt.mods[0][1].virtual_stack_count = 5;
        if (mode == 6) bt.mods[0][0].pokemon_id = -1;
        if (mode == 7) { bt.mods[0][0].virtual_stack_count = 1; bt.mods[0][1].virtual_stack_count = 1; }
        if (mode == 8) bt.money = 9007199254740990.0;
        if (mode == 9) bt.n_mods[0] = 1;
        if (mode == 10) { bt.n_mods[0] = 1; bt.n_mods[1] = 1; bt.mods[1][0] = bt.mods[0][1]; }
        pr_modifier original[PR_MAX_MODIFIERS]; memcpy(original, bt.mods[0], sizeof original);
        pr_rng g = bt.rng, b = bt.battle_rng;
        pr_apply_damage_money_modifiers(&bt, mode == 11 ? NULL : &p, 7);
        double want = mode == 6 ? 103 : mode == 7 ? 109 : mode == 8 ? 9007199254740991.0
            : mode == 9 || mode == 10 ? 103 : 100;
        if (bt.money != want || memcmp(original, bt.mods[0], sizeof original)
            || memcmp(&g, &bt.rng, sizeof g) || memcmp(&b, &bt.battle_rng, sizeof b)) return 1;
        if (mode >= 2 && mode <= 5 && !bt.n_unported) return 1;
        checks++;
    }
    printf("Golden Punch guards: %d checks pass\n", checks);
    return 0;
}
