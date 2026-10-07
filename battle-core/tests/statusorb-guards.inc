/* Refusal/ownership checks complement the upstream full-turn oracle. */
static int orb_guard_checks(void)
{
    int checks = 0;
    for (int side = 0; side < 2; side++) {
        for (int mode = 0; mode < 7; mode++) {
            pr_battle bt = {0}; pr_engine e = {0}; pr_pokemon p = {0};
            p.species = SPECIES_ID_RATTATA; p.id = 10; p.is_player = !side;
            p.hp = p.stats[0] = 100; p.on_field = true;
            p.ability = p.base_ability = ABILITY_ID_BALL_FETCH;
            p.ability_override = -1; p.custom_passive = -1; p.fusion_ability_index = -1;
            bt.field[side * 2] = &p; bt.engine = &e; e.current = -1; e.n_levels = 1;
            bt.n_mods[side] = 1;
            pr_modifier *m = &bt.mods[side][0];
            *m = (pr_modifier){.cls = PR_MOD_TURN_STATUS_EFFECT, .pokemon_id = p.id,
                .stack_count = 1, .is_transferable = true, .status_effect = STATUS_EFFECT_TOXIC};
            if (mode == 0) m->pokemon_id++;
            if (mode == 1) p.hp = 0;
            if (mode == 2) m->status_effect = STATUS_EFFECT_SLEEP;
            if (mode == 3) m->unknown_subclass = true;
            if (mode == 4) p.status = STATUS_EFFECT_BURN;
            if (mode == 5) m->pokemon_id = -1;
            if (mode == 6) m->status_effect = STATUS_EFFECT_BURN;
            pr_modifier original = *m;
            pr_apply_turn_status_modifiers(&bt, &e, &p);
            int queued = 0;
            for (int k = 0; k < PR_MAX_PHASES; k++)
                if (e.used[k] && e.pool[k].type == PH_OBTAIN_STATUS_EFFECT) {
                    queued++;
                    if (e.pool[k].source_p != &p || e.pool[k].ose_effect != m->status_effect) return 1;
                }
            if (queued != (mode >= 5 ? 1 : 0) || memcmp(&original, m, sizeof original)) return 1;
            if ((mode == 2 || mode == 3) && bt.n_unported != 1) return 1;
            checks++;
        }
    }
    printf("orb guards: %d checks pass\n", checks);
    return 0;
}
