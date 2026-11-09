/* A Trainer's Test (src/data/mystery-encounters/encounters/a-trainers-test-encounter.ts, upstream 8555c08).
 *
 * onInit (the wave's stream): randSeedInt(5) picks one of the five stat trainers: 0 Buck, 1 Cheryl, 2 Marley, 3 Mira, 4 Riley (upstream's switch has Buck as its
 * default and the others as cases 1..4). The trainer's config is cloned with levelAdditiveModifier 1.
 *
 * Option 1 (Accept the Challenge): `await transitionMysteryEncounterIntroVisuals()` comes before everything else, so what follows runs on the RUN's RNG, not the
 * option's stream: setEncounterRewards({RELIC_GOLD, ROGUE, ROGUE, fillRemaining}, [an Epic Egg]), then initBattleWithEnemyConfig: the trainer's gender
 * (partyConfig.female == null: !!randSeedInt(2)), the party levels, and the stat trainer's six members (TrainerConfig.initForStatTrainer: the Elite Four template, a
 * static party, a boss, each slot's partyMemberFunc closure; run-core's evil_team.c has their postProcess ops). The win is handleMysteryEncounterVictory for a
 * trainer battle: BattleEndPhase, TrainerVictoryPhase, then the rewards phase with the shop and the egg.
 *
 * Option 2 (Refuse): PartyHealPhase(true), a Rare Egg, the heal-only shop ({fillRemaining: false, rerollMultiplier: -1}), then leaveEncounterWithoutBattle.
 * Both options' outro says who gave which egg.
 */
#include "internal.h"
#include "gc/gacha.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const struct { int16_t trainer; const char *key; } STAT_TRAINERS[5] = {
    {TRAINER_TYPE_BUCK, "buck"}, {TRAINER_TYPE_CHERYL, "cheryl"}, {TRAINER_TYPE_MARLEY, "marley"}, {TRAINER_TYPE_MIRA, "mira"}, {TRAINER_TYPE_RILEY, "riley"},
};

const char *gc_trtest_key(int pick) { return STAT_TRAINERS[pick >= 0 && pick < 5 ? pick : 0].key; }

/* onInit, on the wave's stream; a local copy, so asking (the screen's texts do) draws nothing from the run */
int gc_trtest_pick(const gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    return pr_rng_seed_int(&r, 5, 0);
}

enum { EGG_RARE = 1, EGG_EPIC = 2, EGG_SOURCE_EVENT = 4 };

/* initBattleWithEnemyConfig for the stat trainer (false: a party run-core cannot make) */
static bool build_fight(gc_run *run, int pick)
{
    gc_internal *in = gc_int(run);
    const int t = STAT_TRAINERS[pick].trainer;
    const bool female = pr_rng_seed_int(&run->run.rng, 2, 0) != 0;   /* partyConfig.female == null: !!randSeedInt(2) */
    run->trainer_type = t;   /* the trainer exists before its Pokemon do (the moveset reads its class) */
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    const pr_party_template *tpl = gc_party_template_of(run, t, 0, run->wave);
    if (!tpl) return false;
    uint8_t strengths[8];
    for (int i = 0; i < tpl->size && i < 8; i++) strengths[i] = (uint8_t)tpl->strengths[i];
    int levels[PR_MAX_PARTY_TEMPLATE];
    const int n = pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, false, levels);
    if (n < 1) return false;
    const int add = (int)fmax(js_round(run->wave / 10.0 * 1.0), 0);   /* levelAdditiveModifier 1 */
    for (int i = 0; i < n; i++) levels[i] += add;
    run->trainer_variant = female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
    run->trainer_name = run->trainer_partner_name = -1;
    in->custom_trainer_name[0] = 0;
    in->custom_team_on = false;
    return gc_gen_trainer_party(run, tpl, t, run->trainer_variant, levels, n);
}

void gc_trtest_execute(gc_run *run, int option)
{
    gc_internal *in = gc_int(run);
    const int pick = gc_trtest_pick(run);
    in->me_post.pending = true;   /* the outro: who gave which egg, after the shop */
    in->me_post.a = GC_ME_A_TRAINERS_TEST;
    in->me_post.b = pick;
    in->trtest.pick = (uint8_t)pick;
    if (option != 0) {
        /* Refuse the challenge: PartyHealPhase(true), the Rare Egg, and the heal-only shop */
        in->me_post.c = EGG_RARE;
        in->trtest.egg = EGG_RARE;
        gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));
        gc_trtest_reward(run);   /* doEncounterRewards: the egg goes into the game data as the shop is queued */
        gc_me_leave_with_heal_shop(run);
        return;
    }
    in->me_post.c = EGG_EPIC;
    in->trtest.egg = EGG_EPIC;
    if (!build_fight(run, pick)) {   /* a trainer nobody can make: nobody fights, the encounter ends as a refusal would */
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        run->n_enemy = 0;
        run->trainer_type = run->trainer_variant = -1;
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        in->me_post.pending = false;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->me_battle = true;
    gc_enemy_finish_party_me(run);   /* no modifierConfigs: the ordinary item rolls (an encounter's trainer fight keeps BattleType.MYSTERY_ENCOUNTER) */
    run->me_catch_allowed = false;
    in->me_reward.active = false;
    in->me_no_stats = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    in->trtest.active = true;
    gc_me_say_trainer_fight(run, in);
}

/* handleMysteryEncounterVictory (trainer battle): BattleEndPhase, TrainerVictoryPhase, then MysteryEncounterRewardsPhase: the shop of
 * setEncounterRewards({guaranteedModifierTypeFuncs: [RELIC_GOLD], guaranteedModifierTiers: [ROGUE, ROGUE], fillRemaining: true}) and the egg */
void gc_trtest_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->trtest.active = false;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    gc_rq_push(run, gc_rp(RP_ME_BEFORE_REWARDS, 0, 0, 0));
    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index("RELIC_GOLD");
    c->tiers[c->n_tiers++] = MODIFIER_TIER_ROGUE;
    c->tiers[c->n_tiers++] = MODIFIER_TIER_ROGUE;
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_with_shop(run);
}

/* doEncounterRewards: the egg (new Egg(options).addEggToGameData(); its tier comes from the option taken) */
void gc_trtest_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const int tier = in->trtest.egg;
    if (!tier) return;
    in->trtest.egg = 0;
    if (!run->sys) return;
    char seed[64];
    snprintf(seed, sizeof seed, "trtest-%d-%d-%s", run->wave, tier, run->battle_seed);
    gc_system_gift_egg_source(run->sys, EGG_SOURCE_EVENT, tier, seed, 0);
}

void gc_trtest_outro(gc_run *run, const gc_rphase *p)
{
    static char name[48];
    snprintf(name, sizeof name, "trainerNames:%s", gc_trtest_key(p->b));
    gc_ev_msg(run, "mysteryEncounters/aTrainersTest:outro", 2, pr_arg_key("statTrainerName", name),
              pr_arg_key("eggType", p->c == EGG_EPIC ? "mysteryEncounters/aTrainersTest:eggTypes.epic" : "mysteryEncounters/aTrainersTest:eggTypes.rare"));
}
