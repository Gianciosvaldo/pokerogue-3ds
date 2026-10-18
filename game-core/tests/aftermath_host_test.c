/* Aftermath and Innards Out in a real run: a wild foe with the ability is knocked out by the lead's
 * Tackle and hurts it (a quarter of its max HP; Innards Out the damage of that last hit), once,
 * with the trigger text. A non-contact knockout, a Magic Guard attacker and Damp on the field leave
 * Aftermath silent and the attacker unhurt. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "Aftermath host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void settle(gc_run *r) { gc_event ev; while (gc_run_next_event(r, &ev)) {} }
static int texts(const char *key)
{
    int n = 0;
    for (int i = 0; i < run.n_battle_events; i++) {
        const pr_event *e = &run.battle_events[i];
        if (e->type == PR_EV_TEXT && e->msg.key && !strcmp(e->msg.key, key)) n++;
    }
    return n;
}

/* a fresh run at wave 1: the lead knows only `move` and has `lead_ability`; the foe, at 1 HP, has `foe_ability` */
static pr_pokemon *start(int move, int lead_ability, int foe_ability, pr_pokemon **foe)
{
    const char *why = NULL;
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "aftermath", 777);
    gc_starter s = {.species = SPECIES_ID_BULBASAUR, .tera_type = -1, .moves = {(uint16_t)move}};
    memset(s.ivs, 15, sizeof s.ivs);
    CHECK(gc_run_set_starters(&run, &s, 1, &why)); settle(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    pr_pokemon *p = run.bt.field[0], *f = run.bt.field[2];
    CHECK(p && f);
    p->moves[0] = (uint16_t)move; p->n_moves = 1; p->pp_used[0] = 0;
    if (lead_ability >= 0) { p->custom_ability = (int16_t)lead_ability; p->ability_override = -1; pr_refresh_ability(p); }
    f->custom_ability = (int16_t)foe_ability; f->ability_override = -1; pr_refresh_ability(f);
    f->moves[0] = MOVE_ID_SPLASH; f->n_moves = 1; f->pp_used[0] = 0;
    f->hp = 1;
    p->hp = p->stats[STAT_HP];
    *foe = f;
    return p;
}

static void fight(void)
{
    const char *why = NULL;
    gc_choice c = {GC_CH_FIGHT, 0, -1, 0};
    run.n_battle_events = 0;
    CHECK(gc_run_choose(&run, &c, &why));
    settle(&run);
}

int main(void)
{
    pr_pokemon *foe;
    pr_pokemon *p = start(MOVE_ID_TACKLE, -1, ABILITY_ID_AFTERMATH, &foe);
    int max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0);
    CHECK(texts("abilityTriggers:postFaintContactDamage") == 1);
    CHECK(p->hp == max - (max / 4 > 1 ? max / 4 : 1));                 /* toDmgValue(maxHp / 4) */

    p = start(MOVE_ID_VINE_WHIP, -1, ABILITY_ID_AFTERMATH, &foe);       /* Vine Whip makes contact too */
    max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0 && texts("abilityTriggers:postFaintContactDamage") == 1 && p->hp < max);

    p = start(MOVE_ID_MAGICAL_LEAF, -1, ABILITY_ID_AFTERMATH, &foe);    /* no contact */
    max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0 && texts("abilityTriggers:postFaintContactDamage") == 0 && p->hp == max);

    p = start(MOVE_ID_TACKLE, ABILITY_ID_MAGIC_GUARD, ABILITY_ID_AFTERMATH, &foe);
    max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0 && texts("abilityTriggers:postFaintContactDamage") == 0 && p->hp == max);

    p = start(MOVE_ID_TACKLE, ABILITY_ID_DAMP, ABILITY_ID_AFTERMATH, &foe);
    max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0 && texts("abilityTriggers:postFaintContactDamage") == 0 && p->hp == max);

    p = start(MOVE_ID_TACKLE, -1, ABILITY_ID_INNARDS_OUT, &foe);
    max = p->stats[STAT_HP];
    fight();
    CHECK(foe->hp == 0 && texts("abilityTriggers:postFaintHpDamage") == 1);
    CHECK(p->hp == max - 1);                                            /* the last hit dealt 1: the foe had 1 HP */

    printf("Aftermath and Innards Out: %d checks passed\n", checks);
    return 0;
}
