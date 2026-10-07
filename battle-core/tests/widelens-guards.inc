/* Ownership, missing holder and unsupported-identity checks for the new helper. */
static int widelens_guard_checks(void)
{
    int checks = 0;
    for (int side = 0; side < 2; side++) {
        for (int mode = 0; mode < 8; mode++) {
            pr_battle bt = {0}; pr_pokemon p = {.id = 10, .is_player = !side};
            bt.n_mods[side] = 1;
            pr_modifier *m = &bt.mods[side][0];
            *m = (pr_modifier){.cls = PR_MOD_MOVE_ACCURACY_BOOSTER, .pokemon_id = p.id,
                .stack_count = 2, .virtual_stack_count = 1, .num = 5, .is_transferable = true};
            if (mode == 0) m->pokemon_id++;
            if (mode == 2) m->unknown_subclass = true;
            if (mode == 3) m->num = 10;
            if (mode == 4) m->pokemon_id = -1;
            if (mode == 6) { bt.mods[1-side][0] = *m; bt.n_mods[1-side] = 1; bt.n_mods[side] = 0; }
            if (mode == 7) bt.n_mods[side] = 0;
            pr_modifier original = *m;
            pr_rng rng_before = bt.rng;
            double accuracy = 85;
            pr_apply_accuracy_booster_modifiers(&bt, &p, mode == 1 ? NULL : &accuracy);
            if (accuracy != ((mode == 4 || mode == 5) ? 100 : 85)) return 1;
            if (memcmp(&original, m, sizeof original) || memcmp(&rng_before, &bt.rng, sizeof rng_before)) return 1;
            if ((mode == 2 || mode == 3) && bt.n_unported != 1) return 1;
            checks++;
        }
    }
    printf("Wide Lens guards: %d checks pass\n", checks);
    return 0;
}
