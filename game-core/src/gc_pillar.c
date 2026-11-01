/* Shared Mysterious Challengers / custom Pillar Men trainer encounter.
 * Ordinary Challengers retain native trainer selection, gender/double variants,
 * constructor-level templates and rewards. Pillar Men filter trainers for their
 * custom teams and replace generated members through the custom hook.
 *
 * This owner is always linked. The private `pillar` state is shared so both
 * encounters bypass ordinary TrainerVictoryPhase and receive encounter rewards.
 * Full generated-party moveset and post-constructor RNG parity remain outside
 * this owner proof; unsupported generation keeps its explicit diagnostic.
 */
#include "internal.h"
#include <math.h>
#include <string.h>

enum { STRENGTH_AVERAGE = 3, STRENGTH_STRONG = 4, STRENGTH_STRONGER = 5 };

bool gc_pillar_offered(const gc_run *run)
{
    const gc_custom_hooks *h = gc_int((gc_run *)run)->custom;
    return h && h->pillar && h->pillar(h->user, run, -1, NULL);
}

/* Mysterious Challengers onInit: no custom-only filtering of double trainers.
 * Query on a local RNG owner so opening or redrawing the menu consumes none. */
bool gc_mc_trainers(const gc_run *run,int types[3],int variants[3])
{
    pr_run preview=run->run;preview.unported=0;
    preview.rng=pr_run_seed_offset_rng(&run->run,run->wave,NULL);
    bool valid=true;
    for(int i=0;i<3;i++) {
        types[i]=pr_run_random_trainer_type(&preview,run->biome,run->wave,i==2);
        if(i==1)for(int retry=0;retry<5&&types[1]==types[0];retry++)
            types[1]=pr_run_random_trainer_type(&preview,run->biome,run->wave,false);
        const pr_trainer_info *t=gc_trainer_info_of(types[i]);
        bool female=t&&t->has_genders&&pr_rng_seed_int(&preview.rng,2,0)!=0;
        variants[i]=t&&t->double_only?TRAINER_VARIANT_DOUBLE:female?TRAINER_VARIANT_FEMALE:TRAINER_VARIANT_DEFAULT;
        valid&=t!=NULL;
    }
    return valid&&preview.unported==0;
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

static void trainer_encounter_execute(gc_run *run, int option, bool pillar)
{
    gc_internal *in = gc_int(run);
    const gc_custom_hooks *h = in->custom;
    static gc_custom_team team;
    memset(&team, 0, sizeof team);
    team.passive = -1;
    if (option < 0 || option > 2 || (pillar && (!h || !h->pillar || !h->pillar(h->user, run, option, &team) || !gc_custom_team_check(&team, NULL)))) {
        gc_me_leave_with_heal_shop(run);
        return;
    }

    /* onInit: the three trainers, on the wave's offset stream */
    const pr_rng saved = run->run.rng;
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    int type[3];
    bool female[3];
    int variants[3]={0};
    if(pillar) {
    type[0] = pick_type(run, false);
    female[0] = type[0] >= 0 && roll_female(run, type[0]);
    type[1] = pick_type(run, false);
    for (int retries = 0; retries < 5 && type[1] >= 0 && type[1] == type[0]; retries++) type[1] = pick_type(run, false);
    female[1] = type[1] >= 0 && roll_female(run, type[1]);
    type[2] = pick_type(run, true);
    female[2] = type[2] >= 0 && roll_female(run, type[2]);
    }else if(!gc_mc_trainers(run,type,variants)) {
        run->run.rng=saved;gc_stub(run,GC_STUB_UNPORTED_ENEMY);gc_me_leave_with_heal_shop(run);return;
    }

    /* the option's own stream (executeWithSeedOffset(wave * 10, 100, 1000)): the battle's draws */
    static const int offset_mult[3] = {10, 100, 1000};
    run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * offset_mult[option], NULL);

    const int t = type[option];
    const pr_party_template *tpl = NULL;
    if (t >= 0) {
        if (option == 0 || (!pillar && option == 1 && gc_trainer_info_of(t)->has_template_func)) {
            const pr_trainer_info *info = gc_trainer_info_of(t);
            const int idx = option==0 && info->n_templates > 1 ? pr_rng_seed_int(&run->run.rng, info->n_templates, 0) : 0;
            tpl = gc_party_template_of(run, t, idx, pillar?run->wave-1:run->wave);
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
        const bool doubles=!pillar&&variants[option]==TRAINER_VARIANT_DOUBLE;
        const int n = pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, doubles, levels);
        const int add = (int)fmax(js_round(run->wave / 10.0 * level_modifier[option]), 0);
        for (int i = 0; i < n; i++) levels[i] += add;
        const int variant = pillar?(female[option] ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT):variants[option];

        run->battle_type = BATTLE_TYPE_TRAINER;
        run->double_battle = doubles;
        run->trainer_type = t;
        run->trainer_variant = variant;
        run->trainer_name = run->trainer_partner_name = -1;
        if(!pillar) {
            in->custom_trainer_name[0]=0;
            const pr_trainer_info *info=gc_trainer_info_of(t);
            if(info->names>=0){
                int count=info->names_male>=0?(variant==TRAINER_VARIANT_FEMALE?info->names_female:info->names_male):info->names;
                if(count>0)run->trainer_name=count>1?pr_rng_seed_int(&run->run.rng,count,0):0;
                else gc_stub(run,GC_STUB_TRAINER_TITLE);
                if(doubles&&!info->double_only){count=info->names_female>=0?info->names_female:info->names;run->trainer_partner_name=count>1?pr_rng_seed_int(&run->run.rng,count,0):count==1?0:-1;}
            }
        }

        in->custom_team_on = false;
        ok = gc_gen_trainer_party(run, tpl, t, variant, levels, n);
        if (!ok && pillar && option == 2 && type[0] >= 0) {
            /* a boss class whose party members run-core cannot make (its closures are the fixed battles'): the normal trainer's class stands
             * in for the AI, money and name, the team is the one that fights anyway */
            run->trainer_type = type[0];
            run->trainer_variant = female[0] ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
            ok = gc_gen_trainer_party(run, tpl, type[0], run->trainer_variant, levels, n);
        }
        if (ok && pillar) {
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
    gc_ev_msg(run, run->double_battle?"battle:trainerAppearedDouble":"battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    const bool sent_out = gc_battle_setup_field(run, true);
    gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon(run, "pokemonName", 0, gc_player_active_index(run)));
    if (run->eng.awaiting_confirm) gc_rq_unshift(run, gc_rp(RP_CHECK_SWITCH, 0, 0, 0));
    in->pillar.active = true;
    in->pillar.option = (int8_t)option;
    in->pillar.mc = !pillar;
    in->at_save_point = false;   /* the encounter's one save is the one at its start */
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

void gc_pillar_execute(gc_run *run,int option){trainer_encounter_execute(run,option,true);}
void gc_mc_execute(gc_run *run,int option){trainer_encounter_execute(run,option,false);}

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
