/* The Mysterious Chest on the host: the parts the upstream differential (mystery_chest_diff_test) cannot compare, because its oracle
 * does not play the battle or the shop screens: each reward band really opens a shop of its tiers and nothing else, the trap's
 * Gimmighoul can be fled from and caught, never asks "Will you switch?", and winning it opens the ordinary rewards; a lone survivor
 * fainted by the trap is game over; leaving opens only the heal shop. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gc_run run;
static gc_system sys;

/* a run at wave 45 with a lead of level `lead` and a second Pokemon of level `second`, the chest on offer, its roll in [lo, hi] */
static void start(int lo, int hi, int lead, int second, bool ask_switch)
{
    for (int n = 0;; n++) {
        char seed[32];
        snprintf(seed, sizeof seed, "chest-host-%d", n);
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
        gc_system_init_defaults(&sys);
        run.sys = &sys;
        run.wave = 45;
        run.biome = BIOME_ID_PLAINS;
        run.ask_switch = ask_switch;
        gc_items_to_battle(&run);
        run.n_party = 3;
        for (int i = 0; i < 3; i++) {
            gc_starter s;
            memset(&s, 0, sizeof s);
            s.species = i == 0 ? SPECIES_ID_PIKACHU : i == 1 ? SPECIES_ID_LAPRAS : SPECIES_ID_ONIX;
            s.tera_type = -1;
            for (int k = 0; k < 6; k++) s.ivs[k] = 15;
            s.moves[0] = MOVE_ID_TACKLE;
            gc_mon_new_player(&run, &run.party[i], &s, i == 0 ? lead : i == 1 ? second : 10);
            run.party[i].pause_evolutions = true;
            run.party_order[i] = (int8_t)i;
        }
        run.battle_type = BATTLE_TYPE_WILD;
        const int roll = gc_chest_roll(&run);
        assert(n < 5000);
        if (roll < lo || roll > hi) continue;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_MYSTERIOUS_CHEST;
        gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_MYSTERIOUS_CHEST, GC_ME_COMMON, 0));
        gc_run_until_screen(&run);
        assert(run.screen.kind == GC_SCREEN_MYSTERY);
        return;
    }
}

static bool message_seen(const char *key)
{
    gc_event e;
    bool seen = false;
    while (gc_run_next_event(&run, &e)) if (e.kind == GC_EV_MESSAGE && e.msg.key && !strcmp(e.msg.key, key)) seen = true;
    return seen;
}

static void open_chest(void)
{
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 0, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
}

static void test_bands_open_their_shop(void)
{
    puts("test_chest_bands_open_their_shop...");
    static const struct { int lo, hi, n, tiers[4]; const char *msg; } band[4] = {
        {75, 99, 4, {MODIFIER_TIER_COMMON, MODIFIER_TIER_COMMON, MODIFIER_TIER_GREAT, MODIFIER_TIER_GREAT}, "mysteryEncounters/mysteriousChest:option.1.normal"},
        {45, 74, 3, {MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, MODIFIER_TIER_ULTRA, 0}, "mysteryEncounters/mysteriousChest:option.1.good"},
        {35, 44, 2, {MODIFIER_TIER_ROGUE, MODIFIER_TIER_ROGUE, 0, 0}, "mysteryEncounters/mysteriousChest:option.1.great"},
        {30, 34, 1, {MODIFIER_TIER_MASTER, 0, 0, 0}, "mysteryEncounters/mysteriousChest:option.1.amazing"},
    };
    for (int b = 0; b < 4; b++) {
        start(band[b].lo, band[b].hi, 60, 50, false);
        open_chest();
        assert(run.screen.kind == GC_SCREEN_REWARD);
        /* only the tiers the chest names, nothing filled in, and nobody fainted */
        assert(run.screen.n_options == band[b].n);
        /* (a luck upgrade may raise one by a tier: upgrade_count says how many) */
        for (int i = 0; i < band[b].n; i++) assert(run.screen.options[i].tier - run.screen.options[i].upgrade_count == band[b].tiers[i]);
        assert(run.party[0].b.hp > 0 && run.party[1].b.hp > 0);
        assert(!run.me_battle);
        assert(message_seen(band[b].msg));
        /* taking nothing goes on to the next wave without a heal shop */
        const char *why;
        gc_choice skip = {GC_CH_SKIP, 0, 0, 0};
        assert(gc_run_choose(&run, &skip, &why));
        assert(run.wave == 46 && run.screen.kind != GC_SCREEN_REWARD);
    }
    puts("ok");
}

static void test_trap_fight(void)
{
    puts("test_chest_trap_fight...");
    start(0, 29, 70, 50, true);   /* the SWITCH battle style asks "Will you switch?" in other fights */
    run.party[1].b.moves[0] = MOVE_ID_BITE;   /* the Gimmighoul is a Ghost: Tackle cannot touch it */
    const int lead_hp_before = run.party[0].b.hp;
    open_chest();
    /* the highest level (the lead) fainted for it; the other fights */
    assert(lead_hp_before > 0 && run.party[0].b.hp == 0 && run.party[0].b.status == STATUS_EFFECT_FAINT);
    assert(run.party[1].b.hp > 0);
    assert(message_seen("mysteryEncounters/mysteriousChest:option.1.bad"));
    /* disableSwitch: no question even in the SWITCH style, and the style is still on for the next fight */
    assert(run.screen.kind == GC_SCREEN_COMMAND);
    assert(run.ask_switch);
    assert(run.n_enemy == 1 && run.enemy[0].b.species == SPECIES_ID_GIMMIGHOUL && run.enemy[0].b.is_boss);
    /* no withFleeAllowed(false): the player may run, and may throw a ball */
    assert(run.screen.can_run);
    assert(run.me_catch_allowed);
    /* winning opens the rewards of any wave */
    run.enemy[0].b.hp = 1;
    const char *why;
    gc_choice fight = {GC_CH_FIGHT, 0, -1, 0};
    for (int guard = 0; guard < 8 && run.screen.kind == GC_SCREEN_COMMAND; guard++) assert(gc_run_choose(&run, &fight, &why));
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(run.screen.n_options >= 3);
    puts("ok");
}

static void test_trap_tie_takes_the_first(void)
{
    puts("test_chest_trap_tie_takes_the_first...");
    start(0, 29, 55, 55, false);
    /* the party order differs from storage: the second stored Pokemon leads */
    run.party_order[0] = 1;
    run.party_order[1] = 0;
    open_chest();
    assert(run.party[1].b.hp == 0 && run.party[0].b.hp > 0);
    puts("ok");
}

static void test_trap_skips_fainted(void)
{
    puts("test_chest_trap_skips_fainted...");
    start(0, 29, 40, 80, false);
    run.party[2].b.hp = 0;
    run.party[2].b.status = STATUS_EFFECT_FAINT;
    run.party[1].b.hp = 0;   /* the highest level is already down: the lead is the one the chest takes */
    run.party[1].b.status = STATUS_EFFECT_FAINT;
    open_chest();
    /* the lead was the only one up: it fainted and nobody is left */
    assert(run.screen.kind == GC_SCREEN_GAME_OVER);
    puts("ok");
}

static void test_leave(void)
{
    puts("test_chest_leave...");
    start(0, 99, 60, 50, false);
    run.party[0].b.hp = 1;
    const char *why;
    gc_choice ch = {GC_CH_MYSTERY, 1, 0, 0};
    assert(gc_run_choose(&run, &ch, &why));
    /* "no rewards": only the heal shop */
    assert(run.screen.kind == GC_SCREEN_REWARD);
    assert(!run.me_battle && run.n_enemy == 0);
    puts("ok");
}

int main(void)
{
    test_bands_open_their_shop();
    test_trap_fight();
    test_trap_tie_takes_the_first();
    test_trap_skips_fainted();
    test_leave();
    puts("all chest host tests passed");
    return 0;
}
