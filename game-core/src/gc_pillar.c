/* The Pillar Men: a mystery encounter of custom runs only (gc/custom.h, pillar), beside Mysterious Challengers. Lorenzo, 2026-10-18..25:
 * three foes of rising difficulty and prizes (Esidisi easy, Wamuu hard, Kars brutal), met as black silhouettes; each fights as a trainer battle
 * of the encounter with his own team, which the app hands in.
 *
 * It is built the way upstream builds Mysterious Challengers (src/data/mystery-encounters/encounters/mysterious-challengers-encounter.ts and
 * initBattleWithEnemyConfig), with the team replacing the generated members (upstream's own EnemyPartyConfig.pokemonConfigs):
 *   - three trainer types of the biome are drawn when the encounter starts (onInit: the wave's offset stream): a normal one, a different one
 *     (up to five retries) and a boss one, each with its gender roll when the class has genders;
 *   - easy: the normal trainer's template, no level bonus; hard: one STRONGER and min(ceil(wave / 20), 5) AVERAGE members, levels +1 per 10
 *     waves; brutal: the Elite Four template (six members), levels +1.5 per 10 waves (level += max(round(wave / 10 * modifier), 0));
 *   - the team is cut to the template's size (the first members fight), the levels are the template's;
 *   - the fight is the encounter's: no trainer money or rewards (handleMysteryEncounterVictory), experience as a trainer battle (x1.5; upstream's
 *     0.9 for the brutal fight is ignored there, as it is a trainer battle), and setEncounterRewards: easy TM_COMMON, TM_GREAT and MEMORY_MUSHROOM;
 *     hard ULTRA, ULTRA, GREAT, GREAT; brutal ROGUE, ROGUE, ULTRA, GREAT; each with fillRemaining.
 * Not upstream parity: the exact random draws of a team's trainer party are not compared (the team replaces it), only the structure above. */
#include "internal.h"
#include <math.h>
#include <string.h>

enum { STRENGTH_AVERAGE = 3, STRENGTH_STRONG = 4, STRENGTH_STRONGER = 5 };

bool gc_pillar_offered(const gc_run *run)
{
    const gc_custom_hooks *h = gc_int((gc_run *)run)->custom;
    return h && h->pillar && h->pillar(h->user, run, -1, NULL);
}

/* arena.randomTrainerType, drawn again when the class cannot fight alone as a team (a double-only class, one without templates) */
static int pick_type(gc_run *run, bool boss)
{
    for (int tries = 0; tries < 8; tries++) {
        const int t = pr_run_random_trainer_type(&run->run, run->biome, run->wave, boss);
        const pr_trainer_info *info = t >= 0 ? gc_trainer_info_of(t) : NULL;
        if (info && !info->double_only && (info->n_templates > 0 || info->has_template_func)) return t;
    }
    return -1;
}

static bool roll_female(gc_run *run, int type)
{
    const pr_trainer_info *info = gc_trainer_info_of(type);
    return info && info->has_genders ? pr_rng_seed_int(&run->run.rng, 2, 0) != 0 : false;
}

/* the Elite Four template as run-core has it (AVERAGE, 3 STRONG, STRONGER, AVERAGE) */
static const pr_party_template *elite_four(void)
{
    static const int8_t want[6] = {STRENGTH_AVERAGE, STRENGTH_STRONG, STRENGTH_STRONG, STRENGTH_STRONG, STRENGTH_STRONGER, STRENGTH_AVERAGE};
    for (int i = 0; i < 41; i++) {
        const pr_party_template *t = &pr_party_templates[i];
        if (t->size == 6 && !memcmp(t->strengths, want, sizeof want)) return t;
    }
    return NULL;
}

/* TrainerPartyCompoundTemplate(TrainerPartyTemplate(1, STRONGER, false, true), TrainerPartyTemplate(min(ceil(wave / 20), 5), AVERAGE, false, true)) */
static const pr_party_template *hard_template(int wave)
{
    static pr_party_template t;
    int avg = (wave + 19) / 20;
    if (avg > 5) avg = 5;
    memset(&t, 0, sizeof t);
    t.size = (int8_t)(1 + avg);
    t.strength = STRENGTH_AVERAGE;
    t.evo_kind = 1;
    t.strengths[0] = STRENGTH_STRONGER;
    for (int i = 0; i < t.size; i++) {
        if (i) t.strengths[i] = STRENGTH_AVERAGE;
        t.member[i] = 6;               /* balanced, normal evolution kind */
        t.offsets[i] = (int8_t)(i ? 1 : 0);
    }
    return &t;
}

static const double level_modifier[3] = {0, 1, 1.5};

void gc_pillar_execute(gc_run *run, int option)
{
    gc_internal *in = gc_int(run);
    const gc_custom_hooks *h = in->custom;
    static gc_custom_team team;
    memset(&team, 0, sizeof team);
    team.passive = -1;
    if (option < 0 || option > 2 || !h || !h->pillar || !h->pillar(h->user, run, option, &team) || !gc_custom_team_check(&team, NULL)) {
        gc_me_leave_with_heal_shop(run);
        return;
    }

    /* onInit: the three trainers, on the wave's offset stream */
    const pr_rng saved = run->run.rng;
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    int type[3];
    bool female[3];
    type[0] = pick_type(run, false);
    female[0] = type[0] >= 0 && roll_female(run, type[0]);
    type[1] = pick_type(run, false);
    for (int retries = 0; retries < 5 && type[1] >= 0 && type[1] == type[0]; retries++) type[1] = pick_type(run, false);
    female[1] = type[1] >= 0 && roll_female(run, type[1]);
    type[2] = pick_type(run, true);
    female[2] = type[2] >= 0 && roll_female(run, type[2]);

    /* the option's own stream (executeWithSeedOffset(wave * 10, 100, 1000)): the battle's draws */
    static const int offset_mult[3] = {10, 100, 1000};
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * offset_mult[option], NULL);

    const int t = type[option];
    const pr_party_template *tpl = NULL;
    if (t >= 0) {
        if (option == 0) {
            const pr_trainer_info *info = gc_trainer_info_of(t);
            const int idx = info->n_templates > 1 ? pr_rng_seed_int(&run->run.rng, info->n_templates, 0) : 0;
            tpl = gc_party_template_of(run, t, idx, run->wave - 1);
        } else if (option == 1) {
            tpl = hard_template(run->wave);
        } else {
            tpl = elite_four();
        }
    }

    bool ok = false;
    if (tpl) {
        uint8_t strengths[8];
        for (int i = 0; i < tpl->size && i < 8; i++) strengths[i] = (uint8_t)tpl->strengths[i];
        int levels[PR_MAX_PARTY_TEMPLATE];
        const int n = pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, false, levels);
        const int add = (int)fmax(js_round(run->wave / 10.0 * level_modifier[option]), 0);
        for (int i = 0; i < n; i++) levels[i] += add;
        const int variant = female[option] ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;

        run->battle_type = BATTLE_TYPE_TRAINER;
        run->double_battle = false;
        run->trainer_type = t;
        run->trainer_variant = variant;
        run->trainer_name = run->trainer_partner_name = -1;
        in->custom_team_on = false;
        ok = gc_gen_trainer_party(run, tpl, t, variant, levels, n);
        if (!ok && option == 2 && type[0] >= 0) {
            /* a boss class whose party members run-core cannot make (its closures are the fixed battles'): the normal trainer's class stands
             * in for the AI, money and name, the team is the one that fights anyway */
            run->trainer_type = type[0];
            run->trainer_variant = female[0] ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
            ok = gc_gen_trainer_party(run, tpl, type[0], run->trainer_variant, levels, n);
        }
        if (ok) {
            if (team.n > n) team.n = (uint8_t)n;
            ok = gc_custom_build_trainer(run, &team);
        }
    }
    run->run.rng = saved;
    if (!ok) {   /* a team or a trainer game-core cannot build: nobody fights, the encounter ends */
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        run->n_enemy = 0;
        run->trainer_type = run->trainer_variant = -1;
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }

    gc_enemy_finish_party(run);
    run->me_battle = true;
    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    gc_event *ev = gc_ev_new(run, GC_EV_TRAINER);
    if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    gc_ev_msg(run, "battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    const bool sent_out = gc_battle_setup_field(run, true);
    gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon(run, "pokemonName", 0, gc_player_active_index(run)));
    if (run->eng.awaiting_confirm) gc_rq_unshift(run, gc_rp(RP_CHECK_SWITCH, 0, 0, 0));
    in->pillar.active = true;
    in->pillar.option = (int8_t)option;
    in->at_save_point = false;   /* the encounter's one save is the one at its start */
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

/* handleMysteryEncounterVictory: BattleEndPhase(victory), then the encounter's rewards (setEncounterRewards) and the next wave */
void gc_pillar_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const int option = in->pillar.option;
    in->pillar.active = false;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));

    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    if (option == 0) {
        static const char *const keys[3] = {"TM_COMMON", "TM_GREAT", "MEMORY_MUSHROOM"};
        for (int i = 0; i < 3; i++) c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index(keys[i]);
    } else if (option == 1) {
        static const int8_t tiers[4] = {2, 2, 1, 1};   /* ULTRA, ULTRA, GREAT, GREAT */
        for (int i = 0; i < 4; i++) c->tiers[c->n_tiers++] = tiers[i];
    } else {
        static const int8_t tiers[4] = {3, 3, 2, 1};   /* ROGUE, ROGUE, ULTRA, GREAT */
        for (int i = 0; i < 4; i++) c->tiers[c->n_tiers++] = tiers[i];
    }
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_with_shop(run);
}
