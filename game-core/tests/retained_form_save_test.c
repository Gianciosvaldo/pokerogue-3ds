/* A Pokemon that evolves into a species with fewer forms keeps its form index (Pokemon.evolve only sets formIndex when the evolution names
 * a form: Burmy's Sandy and Trash Cloak -> Mothim, which has one form, stays at 1; upstream's getSpeciesForm() falls back to the species
 * when the index is out of range, gc_form() does the same). The state is real, so the save has to take it: it used to be refused as
 * damaged on load ("the save is damaged"), losing the run. Checks the lead, its fusion half, and that a form no species could have is still
 * refused. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run, back;
static gc_system sys;
static uint8_t buf[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "retained form FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void to_command_screen(void)
{
    const char *why = NULL;
    gc_system_init_defaults(&sys);
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "retained-form", 11);
    gc_starter st[1];
    memset(st, 0, sizeof st);
    st[0].species = SPECIES_ID_BURMY;
    st[0].tera_type = -1;
    memset(st[0].ivs, 15, 6);
    CHECK(gc_run_set_starters(&run, st, 1, &why));
    run.sys = &sys;
    for (int i = 0; i < 64 && !gc_run_can_save(&run); i++) {
        gc_event e;
        while (gc_run_next_event(&run, &e)) {}
        if (gc_run_can_save(&run)) break;
        const gc_screen *s = gc_run_screen(&run);
        gc_choice c;
        memset(&c, 0, sizeof c);
        if (s->kind == GC_SCREEN_COMMAND) break;
        c.kind = GC_CH_CONFIRM;
        c.a = 0;
        if (!gc_run_choose(&run, &c, &why)) break;
    }
    CHECK(gc_run_can_save(&run));
}

static bool round_trip(void)
{
    const char *why = NULL;
    const size_t len = gc_run_save(&run, buf, sizeof buf);
    CHECK(len > 0);
    memset(&back, 0xA5, sizeof back);
    return gc_run_load(&back, buf, len, &why) && !back.load_dropped;
}

int main(void)
{
    to_command_screen();
    pr_pokemon *p = &run.party[0].b;

    /* Burmy (Sandy Cloak, form 1) evolved into Mothim (one form) */
    p->species = SPECIES_ID_MOTHIM;
    p->form = 1;
    CHECK(gc_species(SPECIES_ID_MOTHIM)->nforms == 1);
    CHECK(gc_form(SPECIES_ID_MOTHIM, 1) == gc_form(SPECIES_ID_MOTHIM, 0));   /* the game itself reads it as form 0 */
    CHECK(round_trip());
    CHECK(back.party[0].b.species == SPECIES_ID_MOTHIM && back.party[0].b.form == 1);

    /* the same on the fusion half (a fused Burmy's half evolves the same way) */
    p->form = 0;
    p->fusion_species = SPECIES_ID_MOTHIM;
    p->fusion_form = 2;
    p->fusion_gender_known = true;
    p->fusion_gender = 0;
    p->fusion_ability_index = 0;
    CHECK(round_trip());
    CHECK(back.party[0].b.fusion_species == SPECIES_ID_MOTHIM && back.party[0].b.fusion_form == 2);

    /* a form index no species has is still refused */
    p->fusion_species = 0;
    p->fusion_form = 0;
    p->form = 255;
    CHECK(!round_trip());
    p->form = 0;
    p->fusion_species = SPECIES_ID_MOTHIM;
    p->fusion_form = 255;
    CHECK(!round_trip());

    printf("retained form save: %u checks, 0 failures\n", checks);
    return 0;
}
