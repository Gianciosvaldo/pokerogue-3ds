#include <math.h>
/* FaintPhase ownership and refusal guards supplement the real full-turn oracle. */
static int reviver_guard_checks(void)
{
    int checks = 0;
    for (int side = 0; side < 2; side++) for (int mode = 0; mode < 6; mode++) {
        pr_battle bt = {0}; pr_engine e = {0}; pr_pokemon p = {0};
        p.species = SPECIES_ID_RATTATA; p.id = 10; p.is_player = !side;
        p.level = 50; p.ivs[0] = 2; p.on_field = true; p.ability_override = -1; p.custom_passive = -1;
        p.fusion_ability_index = -1; p.ability = p.base_ability = ABILITY_ID_BALL_FETCH;
        bt.field[side * 2] = &p; bt.party[side][0] = &p; bt.n_party[side] = 1;
        bt.engine = &e; e.current = -1; e.n_levels = 1; bt.wave_index = 3;
        pr_calculate_stats(&bt, &p); p.hp = 0; p.status = STATUS_EFFECT_BURN;
        p.status_toxic_turns = p.status_sleep_turns = p.status_freeze_turns = 3;
        bt.n_mods[side] = 1;
        pr_modifier *m = &bt.mods[side][0];
        *m = (pr_modifier){.cls = PR_MOD_INSTANT_REVIVE, .pokemon_id = p.id, .stack_count = 1};
        if (mode == 1) m->pokemon_id++;
        if (mode == 2) m->unknown_subclass = true;
        if (mode == 3) p.hp = 1;
        if (mode == 5) m->pokemon_id = -1;
        int ph = pr_phase_new(&e, PH_FAINT, side * 2);
        e.pool[ph].source = -1; e.pool[ph].prevent_instant_revive = mode == 4;
        pr_phase_run(&bt, &e, ph);
        int queued = 0;
        for (int k = 0; k < PR_MAX_PHASES; k++) if (e.used[k] && e.pool[k].type == PH_POKEMON_HEAL) {
            queued++;
            if (e.pool[k].heal_flags != (PR_HEAL_MESSAGE | PR_HEAL_REVIVE)
                || e.pool[k].heal_hp != floor(p.stats[0] / 2.0)
                || strcmp(e.pool[k].msg.key, "modifier:pokemonInstantReviveApply")) return 1;
        }
        bool applied = mode == 0 || mode == 5;
        if (queued != applied || bt.n_mods[side] != !applied) return 1;
        if (applied && (p.hp != 0 || p.status || p.status_toxic_turns != -1
            || p.status_sleep_turns != -1 || p.status_freeze_turns != -1
            || !p.on_field || bt.n_faint_history[side])) return 1;
        if (mode == 2 && !bt.n_unported) return 1;
        if (mode == 3 && (p.hp != 1 || p.status != STATUS_EFFECT_BURN || bt.n_faint_history[side])) return 1;
        checks++;
    }
    printf("Reviver Seed guards: %d checks pass\n", checks);
    return 0;
}
