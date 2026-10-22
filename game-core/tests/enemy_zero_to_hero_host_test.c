/* PR_DEV_ENEMY_ZERO_TO_HERO, a departure from the source pin that lorenzo asked for: a trainer's Palafin that
 * leaves the field with U-turn takes its Hero form (its next entry's message is the player's path, zero_to_hero_host_test). With the
 * flag cleared the same turn keeps the pin's behaviour (the off-field enemy's quiet form change ends early). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static int checks;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "enemy Zero to Hero FAIL line %d: %s\n", __LINE__, #x); exit(1); } checks++; } while (0)

static void drain(void) { gc_event e; while (gc_run_next_event(&run, &e)) {} }
static void heal_party(void) { for (int i = 0; i < run.n_party; i++) run.party[i].b.hp = run.party[i].b.stats[0]; }
static int alive_member(void) { for (int i = 0; i < run.n_party; i++) if (run.party[i].b.hp > 0) return i; return 0; }

/* a level-100 party plays on until a trainer battle with a reserve is at its first COMMAND screen */
static bool trainer_battle(const char *seed)
{
    const char *why;
    memset(&run, 0, sizeof run);
    gc_system_init_defaults(&sys);
    gc_run_new(&run, GAME_MODES_CLASSIC, seed, 7);
    gc_starter team[3];
    memset(team, 0, sizeof team);
    const int species[3] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE};
    for (int i = 0; i < 3; i++) { team[i].species = (uint16_t)species[i]; team[i].tera_type = -1; memset(team[i].ivs, 15, sizeof team[i].ivs); }
    CHECK(gc_run_set_starters(&run, team, 3, &why));
    run.sys = &sys;
    for (int i = 0; i < run.n_party; i++) { run.party[i].b.level = 100; gc_mon_recalc(&run, &run.party[i]); }
    for (int step = 0; step < 30000; step++) {
        drain();
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        switch (s->kind) {
        case GC_SCREEN_COMMAND:
            if (run.battle_type == BATTLE_TYPE_TRAINER && run.n_enemy >= 2 && !run.bt.double_battle) return true;
            if (run.wave > 60) return false;
            heal_party();
            c.kind = GC_CH_FIGHT; c.b = -1;
            for (int i = 0; i < 4; i++) if (s->move_ok[i]) { c.a = (int16_t)i; break; }
            break;
        case GC_SCREEN_TARGET: c.kind=GC_CH_TARGET;c.a=-1;for(int k=2;k<4;k++)if(s->target_ok[k]){c.a=k;break;}if(c.a<0)for(int k=0;k<2;k++)if(s->target_ok[k]){c.a=k;break;}break;
        case GC_SCREEN_SWITCH: c.kind = GC_CH_PICK; c.a = -1; for (int i = 0; i < run.n_party; i++) if (s->pick_ok[i]) { c.a = (int16_t)i; break; } break;
        case GC_SCREEN_LEARN_MOVE: c.kind = GC_CH_FORGET; c.a = -1; break;
        case GC_SCREEN_EVOLVE: c.kind = GC_CH_CANCEL; c.a = 1; break;
        case GC_SCREEN_REWARD: c.kind = GC_CH_SKIP; break;
        case GC_SCREEN_BIOME: c.kind = GC_CH_BIOME; break;
        case GC_SCREEN_MYSTERY: c.kind = GC_CH_MYSTERY; c.b = (int16_t)alive_member(); break;
        default: return false;
        }
        if (!gc_run_choose(&run, &c, &why)) return false;
    }
    return false;
}

static int hero_messages(void)
{
    int n = 0;
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_TEXT && e->msg.key && !strcmp(e->msg.key, "abilityTriggers:postSummonZeroToHero")) n++;
    }
    return n;
}

/* the active foe becomes a Zero Palafin that only knows U-turn; the player's lead only Splashes */
static pr_pokemon *palafin_uturn(bool deviation)
{
    gc_mon *foe = &run.enemy[gc_enemy_active_index(&run)];
    pr_pokemon *p = &foe->b;
    CHECK(run.bt.field[2] == p);
    CHECK(run.bt.deviations & PR_DEV_ENEMY_ZERO_TO_HERO);            /* game-core's real runs have it on */
    if (!deviation) run.bt.deviations &= ~PR_DEV_ENEMY_ZERO_TO_HERO;
    p->species = SPECIES_ID_PALAFIN; p->form = 0; p->fusion_species = 0;
    p->base_ability = p->custom_ability = ABILITY_ID_ZERO_TO_HERO; p->ability_override = -1;
    gc_mon_recalc(&run, foe); pr_refresh_ability(p);
    gc_mon_set_move(foe, 0, MOVE_ID_U_TURN); p->n_moves = 1; p->pp_used[0] = 0;
    gc_mon *me = &run.party[run.party_order[0]];
    gc_mon_set_move(me, 0, MOVE_ID_SPLASH); me->b.n_moves = 1; me->b.pp_used[0] = 0;
    return p;
}

static void fight(void)
{
    const char *why;
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    CHECK(gc_run_choose(&run, &c, &why));
    drain();
}

int main(void)
{
    const char *seed = "enemy-zero-to-hero";
    for (int on = 1; on >= 0; on--) {
        CHECK(trainer_battle(seed));
        pr_pokemon *p = palafin_uturn(on);
        CHECK(p->form == 0 && p->ability == ABILITY_ID_ZERO_TO_HERO);
        run.n_battle_events = 0;
        fight();
        CHECK(run.bt.field[2] != p);                                  /* U-turn took it out */
        CHECK(p->form == (on ? 1 : 0));
        CHECK(hero_messages() == 0);
        CHECK(!p->zero_to_hero_shown);                                /* its message waits for its next entry */
    }
    printf("enemy Zero to Hero deviation: %d checks passed\n", checks);
    return 0;
}
