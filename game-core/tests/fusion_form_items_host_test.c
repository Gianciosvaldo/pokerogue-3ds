/* PR_DEV_FUSION_ABILITIES, Multitype and RKS System: Arceus and Silvally only change form through a Plate or a Memory (the pin reads the
 * primary species' form for every held form item). When one of them is the second Pokemon of a DNA Splicers fusion, the Plate or Memory
 * works on that half: the reward generator offers it, the party menu accepts it, the fusion's form changes (the first species' form
 * does not), and the types follow. Switching the item off in the Manage Items screen puts the half back. A real splice, a real reward
 * screen each time; the flag cleared is the pin (nothing offered, nothing accepted). */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int pr_reward_generate(pr_reward_ctx *ctx, int item);   /* reward-core, not in a public header */

static gc_run r;
static gc_system sys;
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "fusion form items line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void settle(void) { gc_event e; while (gc_run_next_event(&r, &e)) {} }

/* two starters, then the second is spliced into the first; the fusion's main ability comes from the second species */
static gc_mon *fused(int first, int second, unsigned deviations)
{
    const char *why;
    gc_system_init_defaults(&sys);
    gc_run_new(&r, GAME_MODES_CLASSIC, "fusion-form-items", 4242);
    gc_starter st[2];
    memset(st, 0, sizeof st);
    st[0].species = SPECIES_ID_BULBASAUR;                               /* cheap enough for the starter screen ... */
    st[1].species = SPECIES_ID_SQUIRTLE;
    for (int i = 0; i < 2; i++) { st[i].tera_type = -1; memset(st[i].ivs, 15, 6); }
    CHECK(gc_run_set_starters(&r, st, 2, &why));
    r.sys = &sys;
    settle();
    CHECK(r.n_party == 2);
    for (int i = 0; i < 2; i++) {                                       /* ... then the two Pokemon the test wants */
        st[i].species = (uint16_t)(i ? second : first);
        gc_mon_new_player(&r, &r.party[i], &st[i], 50);
        r.bt.party[0][i] = &r.party[i].b;
    }
    r.bt.field[0] = &r.party[0].b;
    CHECK(r.party[0].b.species == first && r.party[1].b.species == second);
    CHECK(r.bt.deviations & PR_DEV_FUSION_ABILITIES);
    r.bt.deviations = deviations;

    gc_rqueue *q = &gc_int(&r)->rq;
    gc_rq_reset(q);
    q->current = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    q->waiting = true;
    gc_rq_push(&r, gc_rp(RP_COMMAND, 0, 0, 0));
    memset(&r.screen, 0, sizeof r.screen);
    r.screen.kind = GC_SCREEN_REWARD;
    r.screen.n_options = 1;
    r.screen.options[0] = (gc_offer){.key = "DNA_SPLICERS", .arg = 0, .cost = 0, .target = GC_TARGET_FUSE};
    gc_choice c = {GC_CH_REWARD, 0, 0, 1};
    CHECK(gc_run_choose(&r, &c, &why));
    CHECK(r.n_party == 1 && r.party[0].b.fusion_species == second);
    r.bt.deviations = deviations;
    gc_mon *m = &r.party[0];
    CHECK(gc_system_set_pokemon_caught(&sys, m, true, false));
    return m;
}

/* a reward screen waiting for a pick */
static void reward_screen(gc_offer o)
{
    gc_rqueue *q = &gc_int(&r)->rq;
    gc_rq_reset(q);
    q->current = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    q->waiting = true;
    memset(&r.screen, 0, sizeof r.screen);
    r.screen.kind = GC_SCREEN_REWARD;
    r.screen.options[0] = o;
    r.screen.n_options = 1;
}

static bool types_are(const pr_pokemon *p, int a, int b)
{
    int8_t types[PR_MAX_TYPES];
    const int n = pr_get_types(p, types, 0);
    return n == 2 && types[0] == a && types[1] == b;
}

static int item_index(int arg)
{
    for (int i = 0; i < r.n_items; i++)
        if (r.items[i].arg == arg && r.items[i].pokemon_id == r.party[0].b.id) return i;
    return -1;
}

/* the reward generator, on the fusion alone: only the form-change items of its two species can come up */
static void generated(int item, int lo, int hi, bool expected)
{
    pr_reward_ctx *ctx = gc_reward_ctx(&r);
    bool any = false;
    for (int k = 0; k < 40; k++) {
        const int arg = pr_reward_generate(ctx, item);
        if (arg == -2) continue;                                        /* GEN_NULL */
        any = true;
        CHECK(arg >= lo && arg <= hi);
    }
    CHECK(any == expected);
}

/* one item on the fusion: Plate/Memory `arg` takes the second species to `form`, whose type is `type` */
static void one_item(int first, int second, int ability, int lo, int hi, int arg, int form, int type, int first_type)
{
    gc_mon *m = fused(first, second, PR_DEV_FUSION_ABILITIES);
    CHECK(m->b.ability == ability);
    CHECK(m->b.fusion_form == 0 && m->b.form == 0);

    /* the generator offers the second species' items, and only with the flag on */
    generated(PR_ITEM_FORM_CHANGE_ITEM, lo, hi, true);
    m = fused(first, second, 0);
    generated(PR_ITEM_FORM_CHANGE_ITEM, lo, hi, false);

    /* the flag cleared: the pin refuses it, the Pokemon is untouched */
    gc_offer offer = {.key = "FORM_CHANGE_ITEM", .arg = arg, .target = GC_TARGET_POKEMON};
    const char *why = NULL;
    CHECK(gc_reward_lifecycle_apply(&r, &offer, 0, &why) == GC_APPLY_REFUSED);
    CHECK(r.n_items == 0 && m->b.fusion_form == 0 && m->b.form == 0);

    /* the flag set: picked from the reward screen, it holds the item and the second species changes form */
    m = fused(first, second, PR_DEV_FUSION_ABILITIES);
    reward_screen(offer);
    gc_choice pick = {GC_CH_REWARD, 0, 0, -1};
    CHECK(gc_run_choose(&r, &pick, &why));
    settle();
    CHECK(r.n_items == 1 && r.items[0].pokemon_id == m->b.id && r.items[0].arg == arg);
    CHECK(m->b.fusion_form == form && m->b.form == 0);                    /* the second half, not the first */
    CHECK(m->b.ability == ability);
    CHECK(types_are(&m->b, first_type, type));
    CHECK(r.bt.party[0][0]->fusion_form == form);

    /* Manage Items switches it off: the half goes back, and on again */
    reward_screen((gc_offer){.key = "POTION", .arg = -1, .target = GC_TARGET_POKEMON});
    const int idx = item_index(arg);
    CHECK(idx >= 0);
    CHECK(gc_form_item_toggle(&r, 0, idx, &why));
    CHECK(m->b.fusion_form == 0 && m->b.form == 0);
    CHECK(gc_form_item_toggle(&r, 0, idx, &why));
    CHECK(m->b.fusion_form == form && m->b.form == 0);
    CHECK(types_are(&m->b, first_type, type));
}

/* the item of the first species is the pin, whichever half carries the ability */
static void first_half_is_the_pin(void)
{
    gc_mon *m = fused(SPECIES_ID_ARCEUS, SPECIES_ID_PIKACHU, PR_DEV_FUSION_ABILITIES);
    gc_offer offer = {.key = "FORM_CHANGE_ITEM", .arg = FORM_CHANGE_ITEM_FIST_PLATE, .target = GC_TARGET_POKEMON};
    const char *why = NULL;
    /* Arceus leads, Pikachu is the second: the ability is Pikachu's, not Multitype, so the pin's condition fails and nothing changes form;
     * the item is still held (a failed activation keeps it) */
    CHECK(gc_reward_lifecycle_apply(&r, &offer, 0, &why) == GC_APPLY_DONE);
    CHECK(m->b.form == 0 && m->b.fusion_form == 0);
}

int main(void)
{
    /* Multitype: Arceus as the second species, Plates; Fighting form 1 */
    one_item(SPECIES_ID_PIKACHU, SPECIES_ID_ARCEUS, ABILITY_ID_MULTITYPE, 163, 179,
             FORM_CHANGE_ITEM_FIST_PLATE, 1, POKEMON_TYPE_FIGHTING, POKEMON_TYPE_ELECTRIC);
    /* RKS System: Silvally as the second species, Memories; Fighting form 1 */
    one_item(SPECIES_ID_PIKACHU, SPECIES_ID_SILVALLY, ABILITY_ID_RKS_SYSTEM, 182, 198,
             FORM_CHANGE_ITEM_FIGHTING_MEMORY, 1, POKEMON_TYPE_FIGHTING, POKEMON_TYPE_ELECTRIC);
    first_half_is_the_pin();
    printf("fusion form items: %u checks\n", checks);
    return 0;
}
