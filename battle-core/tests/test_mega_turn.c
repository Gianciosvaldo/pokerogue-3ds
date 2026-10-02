#include "pr/engine.h"
#include "pr/battle.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    pr_battle bt = {0};
    pr_pokemon p = {0};

    p.id = 100;
    p.species = SPECIES_ID_CHARIZARD;
    p.form = 0;
    p.level = 50;
    p.is_player = true;
    for (int i = 0; i < 6; i++) p.ivs[i] = 31;

    pr_calculate_stats(&bt, &p);
    p.hp = p.stats[STAT_HP];

    assert(!pr_is_mega(&p));

    /* Form 1 is Mega Charizard X */
    pr_change_form(&bt, &p, 1);

    assert(pr_is_mega(&p));
    assert(p.form == 1);
    printf("Mega Evolution test passed: Charizard changed to form %d, is_mega=%d, HP=%d, ATK=%d\n",
           p.form, pr_is_mega(&p), p.hp, p.stats[STAT_ATK]);

    return 0;
}
