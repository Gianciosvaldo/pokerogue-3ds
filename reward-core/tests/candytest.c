#include "pr/reward.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void need(int ok, const char *msg)
{
    if (!ok) { fprintf(stderr, "FAIL: %s\n", msg); _Exit(1); }
}

int main(void)
{
    /* Known pinned-upstream getLevelTotalExp points, including the blended non-medium-fast curves. */
    need(pr_reward_level_total_exp(2, 2) == 8, "medium-fast level 2 EXP");
    need(pr_reward_level_total_exp(100, 2) == 1000000, "medium-fast level 100 EXP");
    need(pr_reward_level_total_exp(1, 3) == 0, "medium-slow level 1 EXP");

    pr_run run;
    memset(&run, 0, sizeof run);
    run.mode = GAME_MODES_CLASSIC;

    pr_reward_ctx ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.run = &run;
    ctx.wave = 10;
    ctx.n_party = 2;
    ctx.party[0].species = SPECIES_ID_BULBASAUR;
    ctx.party[0].level = 5;
    ctx.party[0].friendship = 198;
    ctx.party[1].species = SPECIES_ID_CHARMANDER;
    ctx.party[1].level = 7;
    ctx.party[1].friendship = 210;
    ctx.n_items = 1;
    ctx.items[0].item = PR_ITEM_CANDY_JAR;
    ctx.items[0].stack = 2;
    ctx.items[0].owner = -1;

    need(pr_reward_candy_level_count(&ctx) == 3, "two Candy Jars should make a candy worth 3 levels");
    need(pr_reward_apply_candy(&ctx, PR_ITEM_RARE_CANDY, 0), "Rare Candy apply");
    need(ctx.party[0].level == 8, "Rare Candy amplified level");
    need(ctx.party[0].friendship == 200, "Rare Candy friendship cap");
    need(ctx.n_level_events == 1 && ctx.level_events[0].old_level == 5 && ctx.level_events[0].new_level == 8,
         "Rare Candy LevelUp event");

    need(pr_reward_apply_candy(&ctx, PR_ITEM_RARER_CANDY, -1), "Rarer Candy apply");
    need(ctx.party[0].level == 11 && ctx.party[1].level == 10, "Rarer Candy whole-party amplification");
    need(ctx.party[1].friendship == 210, "friendship already above candy cap must not fall");
    need(ctx.n_level_events == 2, "Rarer Candy queues one LevelUp event per party member");

    puts("candy summary: 9 checks, 0 mismatches");
    return 0;
}
