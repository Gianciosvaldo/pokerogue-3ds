#include <math.h>
/* Bounded identity and ownership checks; arithmetic matches existing pinned helper. */
static int typeboost_guard_checks(void)
{
    int checks = 0;
    for (int mode = 0; mode < 20; mode++) {
        pr_battle bt = {0}; pr_pokemon p = {0};
        p.id = 10; p.is_player = mode != 14;
        int side = p.is_player ? 0 : 1;
        bt.n_mods[side] = 1;
        pr_modifier *m = &bt.mods[side][0];
        *m = (pr_modifier){.cls=PR_MOD_ATTACK_TYPE_BOOSTER,.pokemon_id=p.id,.stack_count=1,.move_type=0,.mult=20*0.01};
        int type = 0; double power = 41.5, want = power;
        if (mode == 0) m->pokemon_id++;
        if (mode == 1) type++;
        if (mode == 2) m->unknown_subclass = true;
        if (mode == 3) m->stack_count = 100;
        if (mode == 4) m->virtual_stack_count = 99;
        if (mode == 5) m->mult = 0.25;
        if (mode == 6) { m->move_type = type = 18; }
        if (mode == 7) m->stack_count = 0;
        if (mode == 8) m->virtual_stack_count = -1;
        if (mode == 9) m->pokemon_id = -1;
        if (mode == 10) m->virtual_stack_count = 98;
        if (mode == 11) { power = want = 0.5; }
        if (mode == 12 || mode == 13) {
            bt.n_mods[side] = 2; bt.mods[side][1] = *m;
            if (mode == 12) bt.mods[side][1].stack_count = 3;
            else m->stack_count = 3;
        }
        if (mode == 15) { bt.mods[1][0] = *m; bt.n_mods[1] = 1; bt.n_mods[0] = 0; }
        if (mode == 18) m->stack_count = 99;
        if (mode == 19) m->move_type = type = 17;
        if (mode == 9 || mode == 14 || mode == 19) want = floor(power * 1.2);
        if (mode == 10 || mode == 18) want = floor(power * (1 + 99 * 0.2));
        if (mode == 12) want = floor(floor(power * 1.2) * 1.6);
        if (mode == 13) want = floor(floor(power * 1.6) * 1.2);
        pr_modifier original[2][PR_MAX_MODIFIERS]; memcpy(original,bt.mods,sizeof original);
        pr_rng g=bt.rng,b=bt.battle_rng;
        pr_apply_attack_type_booster_modifiers(&bt,mode==16?NULL:&p,type,mode==17?NULL:&power);
        if (power!=want || memcmp(original,bt.mods,sizeof original)
            || memcmp(&g,&bt.rng,sizeof g) || memcmp(&b,&bt.battle_rng,sizeof b)) return 1;
        if (mode>=2 && mode<=8 && !bt.n_unported) return 1;
        checks++;
    }
    printf("Attack type guards: %d checks pass\n",checks);
    return 0;
}
