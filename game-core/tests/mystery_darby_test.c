/* D'Arby the Gambler's hand rules (src/gc_darby.c) against lorenzo's spec (2026-11-17): the max-STAB score with the spec's own example, the
 * base-Speed tie-break and the coin, and the cheat: never when the fair card wins or ties, only to a strict winner, to the largest margin
 * (ties by base Speed), 75% of the time. */
#include "../src/internal.h"
#include <assert.h>
#include <stdio.h>

static void seed_rng(pr_rng *r, unsigned n)
{
    char t[24];
    pr_seedstr s;
    snprintf(t, sizeof t, "darby-%u", n);
    pr_seed_from_ascii(&s, t);
    pr_rng_sow(r, &s);
}

static gc_darby_card card(int species) { gc_darby_card c = {(uint16_t)species, 0}; return c; }

static void test_score(void)
{
    const gc_darby_card charizard = card(SPECIES_ID_CHARIZARD), swampert = card(SPECIES_ID_SWAMPERT);
    /* the spec's example: Charizard's best against Swampert is Flying 1x (Fire 0.5x); Swampert's is Water 2x (Ground 0x) */
    assert(gc_darby_score4(&charizard, &swampert) == 4);
    assert(gc_darby_score4(&swampert, &charizard) == 8);
    assert(gc_darby_compare(&swampert, &charizard) == 1 && gc_darby_compare(&charizard, &swampert) == -1);
    /* 4x and 0x and 0.25x come out exact: Grass/Steel? use Ferrothorn vs Charizard (Fire 4x on Grass/Steel), Ghost against Normal 0x */
    const gc_darby_card ferrothorn = card(SPECIES_ID_FERROTHORN), snorlax = card(SPECIES_ID_SNORLAX), gengar = card(SPECIES_ID_GENGAR);
    assert(gc_darby_score4(&charizard, &ferrothorn) == 16);
    const gc_darby_card dusknoir = card(SPECIES_ID_DUSKNOIR);
    assert(gc_darby_score4(&dusknoir, &snorlax) == 0);
    /* Bug against Fire/Flying: 0.5 x 0.5 = 0.25x (a dual-type attacker would take its best type: Beedrill's Poison is 1x) */
    const gc_darby_card pinsir = card(SPECIES_ID_PINSIR);   /* pure Bug */
    (void)gengar;
    assert(gc_darby_score4(&pinsir, &charizard) == 1);
    puts("ok score (the spec's Charizard/Swampert example, 4x, 0x)");
}

static void test_tie_break(void)
{
    /* the same species both sides: equal score, equal Speed: the coin, both ways over seeds */
    const gc_darby_card a = card(SPECIES_ID_PIKACHU), b = card(SPECIES_ID_PIKACHU);
    assert(gc_darby_compare(&a, &b) == 0);
    int wins = 0;
    for (int i = 0; i < 400; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i * 2654435761u + 1);
        wins += gc_darby_player_wins(&a, &b, &r);
    }
    assert(wins > 150 && wins < 250);
    /* equal score, different base Speed: the faster wins, no coin. Raichu (110) against Pikachu (90): Electric vs Electric 0.5x both ways */
    const gc_darby_card raichu = card(SPECIES_ID_RAICHU);
    assert(gc_darby_score4(&raichu, &a) == gc_darby_score4(&a, &raichu));
    assert(gc_darby_base_speed(&raichu) > gc_darby_base_speed(&a));
    assert(gc_darby_compare(&raichu, &a) == 1);
    printf("ok tie-break (base Speed first, the coin gave %d of 400)\n", wins);
}

static void test_cheat(void)
{
    const gc_darby_card venusaur = card(SPECIES_ID_VENUSAUR);   /* the player's pick: Grass/Poison */
    /* fair card Swampert loses to Venusaur (Grass 4x on Water/Ground); the bench: Charizard (Fire 2x) and Moltres? use Talonflame-like: Charizard 2x,
     * Pidgeot (Flying 2x), the rest lose */
    gc_darby_card deck[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_SNORLAX), card(SPECIES_ID_CHARIZARD), card(SPECIES_ID_GOLEM),
                                         card(SPECIES_ID_QUAGSIRE), card(SPECIES_ID_ALAKAZAM)};
    int cheats = 0;
    for (int i = 0; i < 1000; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i * 2654435761u + 7);
        bool cheated;
        const int shown = gc_darby_reveal(deck, 0, &venusaur, &r, &cheated);
        if (cheated) {
            cheats++;
            assert(gc_darby_compare(&deck[shown], &venusaur) == 1);   /* only to a strict winner */
        } else {
            assert(shown == 0);
        }
    }
    assert(cheats > 700 && cheats < 800);
    /* the fair card wins: never a cheat */
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 99);
        bool cheated = true;
        assert(gc_darby_reveal(deck, 2, &venusaur, &r, &cheated) == 2 && !cheated);
    }
    /* nothing on the bench beats the pick: he loses cleanly */
    gc_darby_card weak[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_QUAGSIRE), card(SPECIES_ID_GOLEM), card(SPECIES_ID_RHYDON),
                                         card(SPECIES_ID_GASTRODON), card(SPECIES_ID_WHISCASH)};
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 5);
        bool cheated = true;
        assert(gc_darby_reveal(weak, 0, &venusaur, &r, &cheated) == 0 && !cheated);
    }
    /* the largest margin wins the swap: against Ferrothorn (Grass/Steel), Charizard's Fire is 4x, Golem's Ground 2x */
    const gc_darby_card ferrothorn = card(SPECIES_ID_FERROTHORN);
    gc_darby_card pick[GC_DARBY_DECK] = {card(SPECIES_ID_SWAMPERT), card(SPECIES_ID_GOLEM), card(SPECIES_ID_CHARIZARD), card(SPECIES_ID_QUAGSIRE),
                                         card(SPECIES_ID_GASTRODON), card(SPECIES_ID_WHISCASH)};
    for (int i = 0; i < 200; i++) {
        pr_rng r;
        seed_rng(&r, (uint32_t)i + 11);
        bool cheated;
        const int shown = gc_darby_reveal(pick, 0, &ferrothorn, &r, &cheated);
        if (cheated) assert(shown == 2);
    }
    printf("ok cheat (%d of 1000 when he can, only to a strict winner, never when fair wins or nothing beats the pick, biggest margin)\n", cheats);
}

int main(void)
{
    test_score();
    test_tie_break();
    test_cheat();
    puts("mystery_darby_test: all passed");
    return 0;
}
