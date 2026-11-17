/* D'Arby the Gambler (Soul Poker): lorenzo's own mystery encounter for Bizarre runs (his spec, 2026-11-17; custom policy, no upstream).
 * This file holds the hand's rules, which the spec fixes completely:
 *   Score(A -> B) = max over A's types t of the product over B's types d of TypeChart[t][d] ("maximum STAB effectiveness");
 *   the higher score wins; equal scores: the higher base Speed wins; equal base Speed too: a coin flip.
 *   D'Arby's fair card is one of his six, drawn before the player picks. After the pick: if the fair card wins (after the Speed tie-break and the
 *   coin), he plays fair. If it loses and a card on his bench beats the player's pick strictly (score, then base Speed; a coin flip is no win),
 *   he cheats with 75% and swaps to the card with the largest score margin (ties: the highest base Speed), else he plays fair.
 * The encounter flow (wagers, deck, reveal, accusation, rewards) is built on these. */
#include "internal.h"
#include <string.h>

#define DARBY_CHEAT_PERCENT 75

/* the types of a card (species, form): 1 or 2 */
static int card_types(const gc_darby_card *c, int8_t t[2])
{
    const pr_form *f = gc_form(c->species, c->form);
    if (!f) return 0;
    t[0] = f->type1;
    if (f->type2 < 0 || f->type2 == f->type1) return 1;
    t[1] = f->type2;
    return 2;
}

int gc_darby_base_speed(const gc_darby_card *c)
{
    const pr_form *f = gc_form(c->species, c->form);
    return f ? f->base_stats[STAT_SPD] : 0;
}

/* the score in quarters (0, 1 = 0.25x, 2 = 0.5x, 4 = 1x, 8 = 2x, 16 = 4x): exact, so equal scores compare equal */
int gc_darby_score4(const gc_darby_card *a, const gc_darby_card *b)
{
    int8_t ta[2], tb[2];
    const int na = card_types(a, ta), nb = card_types(b, tb);
    int best = 0;
    for (int i = 0; i < na; i++) {
        int product = 4;   /* 1x in quarters */
        for (int k = 0; k < nb; k++) product = product * pr_type_chart_x2[ta[i]][tb[k]] / 2;   /* x2 values 0, 1, 2, 4: exact in quarters for two types */
        if (product > best) best = product;
    }
    return best;
}

/* +1 a wins, -1 b wins, 0 a tie down to the base Speed (the coin decides) */
int gc_darby_compare(const gc_darby_card *a, const gc_darby_card *b)
{
    const int sa = gc_darby_score4(a, b), sb = gc_darby_score4(b, a);
    if (sa != sb) return sa > sb ? 1 : -1;
    const int va = gc_darby_base_speed(a), vb = gc_darby_base_speed(b);
    if (va != vb) return va > vb ? 1 : -1;
    return 0;
}

/* the card D'Arby turns over against `player`: his fair one, or a swap. *cheated says which. `r` gives the cheat roll (randSeedInt(100)) */
int gc_darby_reveal(const gc_darby_card deck[GC_DARBY_DECK], int fair, const gc_darby_card *player, pr_rng *r, bool *cheated)
{
    *cheated = false;
    if (fair < 0 || fair >= GC_DARBY_DECK) return fair;
    if (gc_darby_compare(&deck[fair], player) >= 0) return fair;   /* it wins or ties: no reason to cheat (the coin settles a tie) */
    int best = -1, best_margin = 0, best_speed = 0;
    for (int i = 0; i < GC_DARBY_DECK; i++) {
        if (i == fair || gc_darby_compare(&deck[i], player) <= 0) continue;   /* only a strict winner is a cheat target */
        const int margin = gc_darby_score4(&deck[i], player) - gc_darby_score4(player, &deck[i]);
        const int speed = gc_darby_base_speed(&deck[i]);
        if (best < 0 || margin > best_margin || (margin == best_margin && speed > best_speed)) {
            best = i;
            best_margin = margin;
            best_speed = speed;
        }
    }
    if (best < 0) return fair;   /* nothing on his bench beats the pick: he loses cleanly */
    if (pr_rng_seed_int(r, 100, 0) >= DARBY_CHEAT_PERCENT) return fair;   /* 25%: he takes the fair loss */
    *cheated = true;
    return best;
}

/* the hand: true when the player wins it; a tie down to the base Speed is the coin's (randSeedInt(2) on `r`) */
bool gc_darby_player_wins(const gc_darby_card *player, const gc_darby_card *darby, pr_rng *r)
{
    const int c = gc_darby_compare(player, darby);
    if (c) return c > 0;
    return pr_rng_seed_int(r, 2, 0) == 0;
}
