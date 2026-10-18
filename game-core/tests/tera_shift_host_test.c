/* Tera Shift in a real run: a Terapagos lead takes its Terastal form quietly as the battle opens, its ability
 * becomes Tera Shell (308) through the form's ability slot, and a stat in the new form is recalculated. A non-Terapagos
 * lead with Tera Shift (an ability override) keeps its form. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "Tera Shift host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void settle(gc_run *r) { gc_event ev; while (gc_run_next_event(r, &ev)) {} }

static pr_pokemon *start(int species)
{
    const char *why = NULL;
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "tera-shift", 777);
    gc_starter s = {.species = (uint16_t)species, .tera_type = -1};
    memset(s.ivs, 15, sizeof s.ivs);
    CHECK(gc_run_set_starters(&run, &s, 1, &why)); settle(&run);
    CHECK(run.screen.kind == GC_SCREEN_COMMAND);
    return run.bt.field[0];
}

static const char *form_key(pr_pokemon *p)
{
    const pr_form *f = pr_pokemon_base_form(p);
    return f && f->form_key ? f->form_key : "";
}

int main(void)
{
    pr_pokemon *p = start(SPECIES_ID_TERAPAGOS);
    CHECK(p->species == SPECIES_ID_TERAPAGOS);
    CHECK(!strcmp(form_key(p), "terastal"));                    /* Tera Shift: "" -> terastal on entering the field */
    CHECK(p->ability == 308 && p->base_ability == 308);          /* the Terastal form's slot holds Tera Shell */
    int shown = 0;
    for (int i = 0; i < run.n_battle_events; i++)
        if (run.battle_events[i].type == PR_EV_TEXT && run.battle_events[i].msg.key
            && strstr(run.battle_events[i].msg.key, "teraShift")) shown++;
    CHECK(shown == 0);                                           /* the form change is quiet */

    p = start(SPECIES_ID_BULBASAUR);
    CHECK(!strcmp(form_key(p), ""));
    printf("Tera Shift: %d checks passed\n", checks);
    return 0;
}
