/* The run-level phases, one handler each: src/phases/ from the CommandPhase to the GameOverPhase,
 * minus what only draws (the messages are events, the rest is the app's). gc_phase_start runs a phase
 * and returns true when it is done, false when it waits on a screen; gc_phase_choose takes the answer
 * to that screen (returns true when the phase is done, *ok = false when the answer isn't legal). */
#include "internal.h"

static gc_mon *pm(gc_run *run, int idx) { return &run->party[idx]; }

/* The battle is won: what VictoryPhase queues after the BattleEndPhase it pushes (battle-core's pr_continue
 * pushed that one, and the turn's rest ran before it). Reached when battle-core stops at that BattleEndPhase. */
static void queue_battle_end(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (gc_rq_has(run, RP_TRAINER_VICTORY)) return;   /* upstream's "temporary hotfix": hasPhaseOfType("TrainerVictoryPhase") */
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    if (run->battle_type == BATTLE_TYPE_TRAINER) gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    const int mode = run->run.mode, wave = run->wave;
    const bool endless = mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS;
    const bool classic = mode == GAME_MODES_CLASSIC;
    if (endless || !pr_run_is_wave_final(mode, wave)) {
        /* EggLapsePhase: increment egg hatch step counters */
        if (run->sys) {
            gc_system_increment_egg_steps(run->sys);
        }
        if (classic && wave == 165) {
            gc_rphase p = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0); p.key = "LOCK_CAPSULE"; gc_rq_push(run, p);
        }
        if (wave % 10) {
            gc_rq_push(run, gc_rp(RP_SELECT_MODIFIER, 0, 0, 0));
        } else if (mode == GAME_MODES_DAILY) {
            gc_rphase p = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0); p.key = "EXP_CHARM"; gc_rq_push(run, p);
            if (wave > 10 && !pr_run_is_wave_final(mode, wave)) { p.key = "GOLDEN_POKEBALL"; gc_rq_push(run, p); }
        } else {
            const int super_exp_wave = endless ? 10 : run->run.offset_gym ? 0 : 20;
            gc_rphase p = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
            if (endless && wave == 10) { p.key = "EXP_SHARE"; gc_rq_push(run, p); }
            if (classic && wave == 10) { p.key = "EXP_CHARM"; gc_rq_push(run, p); }
            if (wave <= 750 && (wave <= 500 || wave % 30 == super_exp_wave)) {
                p.key = wave % 30 != super_exp_wave || wave > 250 ? "EXP_CHARM" : "SUPER_EXP_CHARM";
                gc_rq_push(run, p);
            }
            if (wave <= 150 && !(wave % 50)) { p.key = "GOLDEN_POKEBALL"; gc_rq_push(run, p); }
            if (endless && !(wave % 50)) {
                p.key = wave % 250 ? "VOUCHER_PLUS" : "VOUCHER_PREMIUM";
                gc_rq_push(run, p);
            }
        }
        if (gc_is_new_biome(run, wave)) gc_rq_push(run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
        gc_rq_push(run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
    } else {
        run->battle_type = BATTLE_TYPE_CLEAR;
        run->score += pr_run_get_clear_score_bonus(&run->run);
        gc_rq_push(run, gc_rp(RP_GAME_OVER, 1, 0, 0));
    }
    (void)in;
}

/* battle-core stopped: queue what upstream would run next */
static void after_stop(gc_run *run, int stop)
{
    gc_internal *in = gc_int(run);
    switch (stop) {
    case PH_TURN_INIT:
        break;   /* the queue is empty: the next turn */
    case PH_SWITCH:
        if (run->eng.awaiting_pick) gc_rq_unshift(run, gc_rp(RP_SWITCH, 0, 0, 0));
        break;
    case PH_VICTORY: {
        /* an enemy fainted (or was caught): its VictoryPhase, with the experience, then the rest of the turn.
         * A trainer's next Pokemon is battle-core's: FaintPhase pushed its SwitchSummonPhase, which pr_continue
         * runs where upstream does, after the turn's end-of-turn phases. */
        const pr_pokemon *p = run->bt.field[run->eng.stop_phase.battler >= 0 ? run->eng.stop_phase.battler : 2];
        int idx = -1;
        for (int i = 0; i < run->n_enemy; i++) if (&run->enemy[i].b == p) idx = i;
        /* a Pokemon that was caught: AttemptCapturePhase.catch() runs first (the dex, the party, the held items)
         * and unshifts the VictoryPhase when it is done */
        if (gc_catch_is_catch(run, idx)) gc_rq_unshift(run, gc_rp(RP_CATCH, idx, 0, 0));
        gc_rq_unshift(run, gc_rp(RP_VICTORY, idx, 0, 0));
        in->battle_won = true;
        break;
    }
    case PH_GAME_OVER:
        /* GameOverPhase is unshifted: the rest of the turn never runs, upstream ends the run there */
        gc_rq_clear(run);
        gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0));
        break;
    case PH_BATTLE_END:
        /* after a VictoryPhase it is the victory's BattleEndPhase, followed by the wave's rewards */
        if (in->battle_won) {
            in->battle_won = false;
            queue_battle_end(run);
            break;
        }
        /* else the wild Pokemon left the field (Roar, Whirlwind, Emergency Exit...): BattleEndPhase(false),
         * then the next wave */
        /* fall through */
    case PH_SELECT_MODIFIER: case PH_SELECT_BIOME: case PH_NEW_BATTLE:
        gc_rq_push(run, gc_rp(RP_BATTLE_END, 0, 0, 0));
        if (gc_is_new_biome(run, run->wave)) gc_rq_push(run, gc_rp(RP_SELECT_BIOME, 0, 0, 0));
        gc_rq_push(run, gc_rp(RP_NEW_BATTLE, 0, 0, 0));
        break;
    default:
        break;
    }
}

/* ---- applyPartyExp ---- */
/* The party as applyPartyExp reads it. participants = the Set of participant ids; the caller sets trainer_battle and
 * me_exp_multiplier (NAN: not a mystery encounter's battle) */
void gc_exp_party_build(gc_run *run, pr_exp_party *pp, const uint32_t *participants, int n_participants)
{
    memset(pp, 0, sizeof *pp);
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = pm(run, run->party_order[k]);
        pp->party[k].id = m->b.id;
        pp->party[k].level = m->b.level;
        pp->party[k].hp = m->b.hp;
        pp->party[k].pokerus = m->pokerus;
        for (int i = 0; i < n_participants; i++) pp->party[k].participated |= participants[i] == m->b.id;
    }
    pp->n_party = run->n_party;
    pp->n_participants = n_participants;
    pp->max_exp_level = pr_run_get_max_exp_level(&run->run, run->wave, false);
    pp->trainer_battle = run->battle_type == BATTLE_TYPE_TRAINER;
    pp->me_exp_multiplier = NAN;
    pp->exp_share_stacks = gc_item_stack(run, "EXP_SHARE");
    pp->exp_balance_stacks = gc_item_stack(run, "EXP_BALANCE");
    pp->oval_charm_stacks = gc_item_stack(run, "OVAL_CHARM");
    for (int i = 0; i < run->n_items && pp->n_eggs < 16; i++) {
        const gc_item *it = &run->items[i];
        double boost = !strcmp(it->key, "LUCKY_EGG") ? 40 : !strcmp(it->key, "GOLDEN_EGG") ? 100 : 0;
        if (boost > 0 && it->pokemon_id) {
            pp->eggs[pp->n_eggs].pokemon_id = it->pokemon_id;
            pp->eggs[pp->n_eggs].stacks = it->stack;
            pp->eggs[pp->n_eggs].boost_percent = boost;
            pp->n_eggs++;
        }
    }
}

/* the phases applyPartyExp unshifts: an ExpPhase / ShowPartyExpBarPhase per member that gets experience */
void gc_exp_party_queue(gc_run *run, const pr_exp_party *pp, double exp_value, bool use_wave_index_multiplier)
{
    int idx[PR_MAX_EXP_PARTY];
    double amount[PR_MAX_EXP_PARTY];
    const int n = pr_run_apply_party_exp(pp, exp_value, use_wave_index_multiplier, run->wave, idx, amount);
    for (int i = 0; i < n; i++) {
        if (!amount[i]) continue;
        const int storage = run->party_order[idx[i]];
        gc_rphase p = gc_rp(RP_EXP, storage, pm(run, storage)->b.on_field, 0);
        p.d = amount[i];
        gc_rq_unshift(run, p);
    }
}

/* ---- VictoryPhase ---- */
static void victory(gc_run *run, int enemy_idx)
{
    pr_battle *bt = &run->bt;
    if (enemy_idx >= 0) {
        const pr_pokemon *e = &run->enemy[enemy_idx].b;
        const pr_form *f = gc_form(e->species, e->form);
        /* getExpValue(): getBaseExp() (x1.5 for mega, primal and gigantamax forms) * level / 5 + 1 */
        const double base = f->exp_boost ? f->base_exp * 1.5 : f->base_exp;
        const double exp_value = base * e->level / 5 + 1;
        /* applyPartyExp(expValue, pokemonDefeated = true) */
        pr_exp_party pp;
        gc_exp_party_build(run, &pp, bt->participants, bt->n_participants);
        /* Participating survivors gain friendship through the bounded Bell
         * projection. Unrepresented stock retains the counted legacy gain. */
        if (bt->n_participants > 0)
            for (int k = 0; k < run->n_party; k++) {
                gc_mon *m = pm(run, run->party_order[k]);
                if (m->b.hp > 0 && pp.party[k].participated
                    && !gc_soothe_battle_gain(run,run->party_order[k])) {
                    gc_stub(run,GC_STUB_REWARD_APPLY);
                    gc_system_add_friendship(run->sys, run, m, 3, false);
                }
            }
        gc_exp_party_queue(run, &pp, exp_value, false);
    }
}

/* ---- ExpPhase / ShowPartyExpBarPhase ---- */
static void exp_phase(gc_run *run, gc_rphase *p)
{
    gc_mon *m = pm(run, p->a);
    const double exp = floor(gc_apply_exp_boosters(run, p->d));
    if (p->b) gc_ev_msg(run, "battle:expGain", 2, gc_m_mon(run, "pokemonName", 0, p->a), pr_arg_int("exp", (int)exp));
    const int last = m->b.level;
    gc_mon_add_exp(run, m, exp);
    gc_event *ev = gc_ev_new(run, GC_EV_EXP);
    if (ev) { ev->a = p->a; ev->b = (int32_t)exp; ev->c = last; }
    if (m->b.level > last) gc_rq_unshift(run, gc_rp(RP_LEVEL_UP, p->a, last, m->b.level));
}

/* ---- LevelUpPhase ---- */
static void level_up(gc_run *run, gc_rphase *p)
{
    gc_mon *m = pm(run, p->a);
    gc_mon_recalc(run, m);
    gc_event *ev = gc_ev_new(run, GC_EV_LEVEL_UP);
    if (ev) { ev->a = p->a; ev->b = p->c; }
    gc_ev_msg(run, "battle:levelUp", 2, gc_m_mon(run, "pokemonName", 0, p->a), pr_arg_int("level", p->c));
    /* end(): the new level moves, then the evolution check */
    if (p->b < 100) {
        uint16_t mv[32];
        const int n = gc_level_up_moves(m, p->b + 1, mv, 32);
        for (int i = 0; i < n; i++) gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, p->a, mv[i], 0));
        /* the prevolutions' moves only get a message (battle:prevoMovePrompt): their learnsets aren't
         * walked yet */
    }
    if (!m->pause_evolutions) {
        const int evo = gc_mon_get_evolution(run, m);
        if (evo >= 0) gc_rq_unshift(run, gc_rp(RP_EVOLUTION, p->a, evo, p->b));
    }
}

/* ---- LearnMovePhase ---- */
static void learn(gc_run *run, int idx, int move, int slot)
{
    gc_mon *m = pm(run, idx);
    gc_mon_set_move(m, slot, move);
    gc_event *ev = gc_ev_new(run, GC_EV_MOVE_LEARNED);
    if (ev) { ev->a = idx; ev->b = slot; ev->c = move; }
    gc_ev_msg(run, "battle:learnMove", 2, gc_m_mon(run, "pokemonName", 0, idx), pr_arg_move("moveName", move));
}

static bool learn_move_start(gc_run *run, gc_rphase *p)
{
    gc_mon *m = pm(run, p->a);
    const int move = p->b;
    if (pr_moves[move].unimplemented) return true;
    for (int i = 0; i < PR_N_MOVES(&m->b); i++) if (PR_MOVES(&m->b)[i] == move && move != MOVE_ID_SKETCH) return true;
    if (PR_N_MOVES(&m->b) < 4) { learn(run, p->a, move, PR_N_MOVES(&m->b)); return true; }
    gc_ev_msg(run, "battle:learnMovePrompt", 2, gc_m_mon(run, "pokemonName", 0, p->a), pr_arg_move("moveName", move));
    gc_ev_msg(run, "battle:learnMoveLimitReached", 1, gc_m_mon(run, "pokemonName", 0, p->a));
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_LEARN_MOVE;
    s->party = (int8_t)p->a;
    s->move = (uint16_t)move;
    return false;
}

/* ---- EvolutionPhase ---- */
static bool evolution_start(gc_run *run, gc_rphase *p)
{
    gc_mon *m = pm(run, p->a);
    gc_ev_msg(run, "menu:evolving", 1, gc_m_mon(run, "pokemonName", 0, p->a));
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_EVOLVE;
    s->party = (int8_t)p->a;
    s->species = gc_evo_data[p->b].species;
    (void)m;
    return false;
}

static void evolve(gc_run *run, gc_rphase *p)
{
    gc_mon *m = pm(run, p->a);
    const int before = m->b.species;
    const pr_msg_arg pre_name = gc_m_mon(run, "pokemonName", 0, p->a);
    gc_mon_evolve(run, m, p->b);
    gc_event *ev = gc_ev_new(run, GC_EV_EVOLVED);
    if (ev) { ev->a = p->a; ev->b = before; ev->c = m->b.species; }
    gc_ev_msg(run, "menu:evolutionDone", 2, pre_name, pr_arg_pokemon_name(&run->bt, "evolvedPokemonName", &m->b));
    /* postEvolve: getLevelMoves({startingLevel: lastLevel + 1, includeEvolutionMoves}) filtered to the evolution moves
     * (level 0) of the evolved form, one LearnMovePhase each */
    uint16_t learned[64];
    const int n_learned = gc_evolution_moves(m, learned, 64);
    for (int i = 0; i < n_learned; i++) gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, p->a, learned[i], 0));
}

/* ---- BattleEndPhase ---- */
/* Battle.pickUpScatteredMoney: every MoneyMultiplierModifier (Amulet Coin) in the list's order adds
 * floor(value * 0.2 * stacks), Happy Hour doubles it, then addMoney and the "picked up" message */
void gc_pick_up_scattered_money(gc_run *run)
{
    pr_battle *bt = &run->bt;
    double amount = bt->money_scattered;
    for (int i = 0; i < run->n_items; i++)
        if (!strcmp(run->items[i].key, "AMULET_COIN")) amount += floor(amount * 0.2 * run->items[i].stack);
    if (pr_arena_has_tag(bt, ARENA_TAG_TYPE_HAPPY_HOUR, ARENA_TAG_SIDE_BOTH)) amount *= 2;
    run->money = pr_run_add_money(run->money, amount);
    gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
    if (ev) { ev->a = (int32_t)amount; ev->b = (int32_t)run->money; }
    gc_ev_msg(run, "battle:moneyPickedUp", 1, pr_arg_int("moneyAmount", (int)amount));
    bt->money_scattered = 0;
}

static void battle_end(gc_run *run, gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    pr_battle *bt = &run->bt;
    bool victory = p->a;
    /* cull any other BattleEndPhase */
    for (;;) {
        gc_rqueue *q = &in->rq;
        bool found = false;
        for (int l = 0; l < q->n_levels && !found; l++)
            for (int k = 0; k < q->n[l]; k++)
                if (q->q[l][k].type == RP_BATTLE_END) { victory |= q->q[l][k].a; found = true; break; }
        if (!found) break;
        gc_rq_remove(run, RP_BATTLE_END);
    }
    if (victory) {
        /* currentBattle.addBattleScore(): Sine.easeIn over the turns taken */
        double mult = run->n_enemy / 2.0 + 0.5;
        for (int i = 0; i < run->n_enemy; i++)
            if (run->enemy[i].b.is_boss) mult *= run->enemy[i].b.boss_segments / 1.5 / run->n_enemy;
        double t = bt->turn - 2 < 10 * mult ? bt->turn - 2 : 10 * mult;
        double v = 1 - t / (10 * mult);
        double ease = v == 0 ? 0 : v == 1 ? 1 : 1 - cos(v * 3.141592653589793 / 2);   /* Phaser's Sine.In */
        run->score += (int)ceil(bt->battle_score * ease);
    }
    /* PostBattleAbAttr: Pickup (PostBattleLootAbAttr) takes an item from the enemies' loot after a victory. battle-core keeps
     * only the loot's count and game-core drops the enemy items at the battle's end, so the transfer is not ported:
     * counted when it would have happened. */
    if (victory && bt->post_battle_loot > 0)
        for (int i = 0; i < run->n_party; i++)
            if (gc_mon_allowed(&run->party[i]) && pr_has_ability_active(bt, &run->party[i].b, ABILITY_ID_PICKUP)) {
                gc_stub(run, GC_STUB_POST_BATTLE);
                break;
            }
    /* BattleEndPhase: currentBattle.pickUpScatteredMoney() (Pay Day) */
    if (bt->money_scattered) gc_pick_up_scattered_money(run);
    /* clearEnemyHeldItemModifiers; then the bounded temporary booster lapse. */
    run->n_enemy_items = 0;
    if (!gc_temp_lapse(run, NULL)) gc_stub(run, GC_STUB_POST_BATTLE);
    if (!gc_lure_lapse(run, NULL)) gc_stub(run, GC_STUB_POST_BATTLE);
    if (!victory && !bt->successful_run && !gc_rq_has(run, RP_SELECT_MODIFIER)) {
        if (bt->enemy_faints > 0) gc_rq_unshift(run, gc_rp(RP_SELECT_MODIFIER, 0, 0, 0));
        else gc_rq_unshift(run, gc_rp(RP_SELECT_MODIFIER, 1, 0, 0));   /* no reroll, no fill: rewards stub */
    }
    in->last_battle_type = run->battle_type;
    in->last_final_boss = run->run.mode == GAME_MODES_CLASSIC && pr_run_is_wave_final(run->run.mode, run->wave);
    in->last_max_exp_level = bt->max_exp_level;
    gc_battle_end_cleanup(run);
}

/* ---- TrainerVictoryPhase / MoneyRewardPhase ---- */
static void trainer_victory(gc_run *run)
{
    const gc_trainer_config *c = gc_trainer_config_of(run->trainer_type);
    gc_rphase p = gc_rp(RP_MONEY_REWARD, 0, 0, 0);
    p.d = c ? c->money_multiplier : 1;   /* config.moneyMultiplier */
    gc_rq_unshift(run, p);
    /* TrainerVictoryPhase's rewards: the config's modifierRewardFuncs, one ModifierRewardPhase each, in order, then the
     * boss voucher. Which items and which trainer has a voucher are reward-core's (pr_reward_trainer_victory_rewards,
     * diff-tested): its items are the reward funcs' first and the voucher's reward last. If reward-core doesn't have
     * the trainer's rewards, they are not given and the stand-in counts it. */
    const int n_funcs = c ? c->n_reward_funcs : 0;
    pr_reward_trainer_victory tv;
    const bool unlocked = gc_system_has_voucher_unlock(run->sys, run->trainer_type);
    if (pr_reward_trainer_victory_rewards(run->trainer_type, unlocked, false, &tv) != 0 || tv.n_items < n_funcs) {
        if (n_funcs) gc_stub(run, GC_STUB_TRAINER_REWARDS);
        tv.voucher_type = -1;
        tv.n_items = 0;
    }
    for (int i = 0; i < n_funcs && i < tv.n_items; i++) {
        gc_rphase rp = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
        rp.key = pr_item_keys[tv.items[i]];
        gc_rq_unshift(run, rp);
    }
    /* The boss voucher: Object.hasOwn(vouchers, key) && !globalScene.validateVoucher(voucher) && config.isBoss.
     * validateVoucher unlocks the voucher now and counts it (the first win of a boss: unlocked, +1 of the voucher's
     * own type, no reward) and returns true, so the ModifierRewardPhase is queued only on a later win, when it was
     * unlocked already. Without system data (sys NULL = no system updates) nothing unlocks and no voucher is given. */
    if (tv.voucher_type >= 0) {
        if (tv.unlock_voucher) {
            gc_system_unlock_voucher(run->sys, run->trainer_type, tv.voucher_type);
        } else if (tv.n_items > n_funcs) {
            gc_rphase vp = gc_rp(RP_MODIFIER_REWARD, 0, 0, 0);
            vp.key = pr_item_keys[tv.items[tv.n_items - 1]];
            gc_rq_unshift(run, vp);
        }
    }
    gc_ev_msg(run, "battle:trainerDefeated", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
}

static void money_reward(gc_run *run, double mult)
{
    int stacks[8], n = 0;
    for (int i = 0; i < run->n_items && n < 8; i++)
        if (!strcmp(run->items[i].key, "AMULET_COIN")) stacks[n++] = run->items[i].stack;
    const double amount = pr_run_get_money_reward(run->wave, mult, stacks, n, false);
    run->money = pr_run_add_money(run->money, amount);
    gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
    if (ev) { ev->a = (int32_t)amount; ev->b = (int32_t)run->money; }
    gc_ev_msg(run, "battle:moneyWon", 1, pr_arg_int("moneyAmount", (int)amount));
}

/* ---- SelectModifierPhase ---- */
/* kind: GC_REWARD_* (gc/ports.h). The options and the reroll price are reward-core's, through gc_reward.c */
static void reward_screen(gc_run *run, int kind)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_REWARD;
    s->n_options = gc_port_reward_options(run, kind, s->options, &s->reroll_cost);
    s->n_shop = 0;
    if (pr_run_get_shop_status(&run->run)) {
        pr_shop_option so[PR_SHOP_ITEM_COUNT_];
        const double base = pr_run_get_wave_money_amount(run->wave, 1);
        const int n = pr_run_get_player_shop_options(&run->run, run->wave, base, so);
        for (int i = 0; i < n && s->n_shop < 16; i++) {
            gc_offer *o = &s->shop[s->n_shop++];
            o->key = pr_shop_item_name[so[i].item];
            o->arg = -1;
            o->tier = 0;
            o->target = gc_reward_target(o->key);
            o->cost = so[i].cost;
        }
    }
    s->can_skip = true;
}

/* SelectModifierPhase.start(): p->a = 1 the no-victory screen, 2 a mystery encounter's custom shop; p->b = a copy of the screen (copy()) */
static bool select_modifier_start(gc_run *run, gc_rphase *p)
{
    reward_screen(run, p->b ? GC_REWARD_COPY : p->a == 2 ? GC_REWARD_CUSTOM : p->a ? GC_REWARD_RESTRICTED : GC_REWARD_FIRST);
    return false;
}

static bool select_modifier_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    gc_internal *in = gc_int(run);
    gc_screen *s = &run->screen;
    switch (c->kind) {
    case GC_CH_SKIP:
        *ok = true;
        return true;
    case GC_CH_REWARD:
        if (c->a < 0 || c->a >= s->n_options) { *why = "no such option"; return false; }
        if (!gc_port_apply_reward(run, &s->options[c->a], c->b, c->c, why)) return false;
        *ok = true;
        return true;
    case GC_CH_SHOP: {
        if (c->a < 0 || c->a >= s->n_shop) { *why = "no such item"; return false; }
        gc_offer o = s->shop[c->a];
        o.is_purchase = true;
        if (run->money < o.cost) { *why = "modifierSelectUiHandler:notEnoughMoney"; return false; }
        if (!gc_port_apply_reward(run, &o, c->b, c->c, why)) return false;
        run->money -= o.cost;
        gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
        if (ev) { ev->a = -(int32_t)o.cost; ev->b = (int32_t)run->money; }
        *ok = true;
        /* a purchase keeps the screen open (applyModifier with a cost doesn't end the phase) */
        return false;
    }
    case GC_CH_REROLL:
        if (s->reroll_cost < 0) { *why = "no reroll here"; return false; }
        if (!pr_reward_try_deduct_money(&run->money, s->reroll_cost)) { *why = "modifierSelectUiHandler:notEnoughMoney"; return false; }
        {
            gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
            if (ev) { ev->a = -(int32_t)s->reroll_cost; ev->b = (int32_t)run->money; }
        }
        reward_screen(run, GC_REWARD_REROLL);
        *ok = true;
        return false;
    case GC_CH_LOCK_RARITIES:
        /* toggleRerollLock(): only where a reroll is allowed, and the button only exists with the Lock Capsule */
        if (s->reroll_cost < 0) { *why = "no reroll here"; return false; }
        if (gc_item_stack(run, "LOCK_CAPSULE") <= 0) { *why = "no Lock Capsule"; return false; }
        in->lock_rarities = !in->lock_rarities;
        s->reroll_cost = gc_reward_reroll_cost(run);
        *ok = true;
        return false;
    default:
        *why = "not a reward choice";
        return false;
    }
}

/* ---- SelectBiomePhase / SwitchBiomePhase / PartyHealPhase ---- */
static void set_next_biome(gc_run *run, int biome)
{
    if ((run->wave + 1) % 10 == 1) {
        /* MoneyInterestModifier (Coin Case) */
        int stacks[8], n = 0;
        for (int i = 0; i < run->n_items && n < 8; i++)
            if (!strcmp(run->items[i].key, "COIN_CASE")) stacks[n++] = run->items[i].stack;
        if (n) {
            const double before = run->money;
            run->money = pr_run_apply_money_interest(run->money, stacks, n);
            gc_event *ev = gc_ev_new(run, GC_EV_MONEY);
            if (ev) { ev->a = (int32_t)(run->money - before); ev->b = (int32_t)run->money; }
        }
        gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));
    }
    gc_rq_unshift(run, gc_rp(RP_SWITCH_BIOME, biome, 0, 0));
}

static bool select_biome_start(gc_run *run, gc_rphase *p)
{
    uint8_t choices[PR_MAX_BIOME_LINKS];
    int n = 0;
    const bool map = gc_item_stack(run, "MAP") > 0;
    const int next = pr_run_select_next_biome(&run->run, &run->depths, run->biome, run->wave, map, choices, &n);
    if (next >= 0) { set_next_biome(run, next); return true; }
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_BIOME;
    s->n_biomes = n;
    memcpy(s->biomes, choices, (size_t)n);
    (void)p;
    return false;
}

static void party_heal(gc_run *run)
{
    for (int i = 0; i < run->n_party; i++) {
        pr_pokemon *b = &run->party[i].b;
        b->hp = b->stats[0];
        b->status = STATUS_EFFECT_NONE;
        b->pending_status = STATUS_EFFECT_NONE;
        b->status_toxic_turns = 0;
        b->status_sleep_turns = b->status_freeze_turns = -1;
        for (int k = 0; k < 4; k++) PR_PP_USED(b)[k] = 0;
        gc_event *ev = gc_ev_new(run, GC_EV_HP);
        if (ev) { ev->a = 0; ev->b = i; ev->c = b->hp; }
    }
}

/* ---------------------------------------------------------------- dispatch */
bool gc_phase_start(gc_run *run, gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    switch (p->type) {
    case RP_NEW_BATTLE: {
        bool any = false;
        for (int i = 0; i < run->n_party; i++) any |= gc_mon_allowed(&run->party[i]);
        if (!any) { gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0)); return true; }
        gc_new_battle(run);
        return true;
    }
    case RP_TURN_INIT:
        gc_battle_turn_init(run);
        if (!run->bt.field[0] || run->bt.field[0]->hp <= 0) {
            /* a fainted lead left on the field (a faint in the turn's lost tail): SwitchPhase */
            bool bench = false;
            for (int i = 0; i < run->n_party; i++) bench |= gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field;
            if (!bench) { gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0)); return true; }
            gc_rq_unshift(run, gc_rp(RP_SWITCH, 1, 0, 0));
            return true;
        }
        gc_rq_unshift(run, gc_rp(RP_COMMAND, 0, 0, 0));
        gc_rq_unshift(run, gc_rp(RP_TURN, 0, 0, 0));
        return true;
    case RP_COMMAND:
        if (gc_battle_queued_command(run)) return true;
        gc_battle_command_screen(run);
        return false;
    case RP_TURN: {
        gc_port_enemy_command(run, 2);
        after_stop(run, gc_battle_run(run, -2));
        return true;
    }
    case RP_SWITCH:
        if (p->a) {
            /* game-core's own prompt for a fainted lead: every allowed bench member, no skipping */
            gc_screen *s = &run->screen;
            memset(s, 0, sizeof *s);
            s->kind = GC_SCREEN_SWITCH;
            for (int i = 0; i < run->n_party; i++) s->pick_ok[i] = gc_mon_allowed(&run->party[i]) && !run->party[i].b.on_field;
            return false;
        }
        gc_battle_switch_screen(run);
        return false;
    case RP_VICTORY:
        victory(run, p->a);
        /* VictoryPhase.end(): the experience phases it unshifted run first, then the turn goes on where the
         * faint interrupted it */
        gc_rq_unshift(run, gc_rp(RP_CONTINUE, 0, 0, 0));
        return true;
    case RP_CONTINUE:
        after_stop(run, gc_battle_continue(run));
        return true;
    case RP_CHECK_SWITCH: {
        gc_screen *s = &run->screen;
        memset(s, 0, sizeof *s);
        s->kind = GC_SCREEN_CHECK_SWITCH;
        return false;
    }
    case RP_EXP:
        exp_phase(run, p);
        return true;
    case RP_LEVEL_UP:
        level_up(run, p);
        return true;
    case RP_LEARN_MOVE:
        return learn_move_start(run, p);
    case RP_EVOLUTION:
        return evolution_start(run, p);
    case RP_REWARD_FORM:
        gc_reward_change_form(run, p->a, p->b);
        return true;
    case RP_BATTLE_END:
        battle_end(run, p);
        return true;
    case RP_TRAINER_VICTORY:
        trainer_victory(run);
        return true;
    case RP_MONEY_REWARD:
        money_reward(run, p->d);
        return true;
    case RP_MODIFIER_REWARD:
        gc_reward_gain(run, p->key);
        return true;
    case RP_SELECT_MODIFIER:
        return select_modifier_start(run, p);
    case RP_SELECT_BIOME:
        pr_run_reset_seed(&run->run, run->wave);
        return select_biome_start(run, p);
    case RP_SWITCH_BIOME: {
        run->biome = p->a;
        gc_event *ev = gc_ev_new(run, GC_EV_BIOME);
        if (ev) ev->a = p->a;
        return true;
    }
    case RP_PARTY_HEAL:
        party_heal(run);
        return true;
    case RP_MYSTERY: {
        gc_screen *s = &run->screen;
        memset(s, 0, sizeof *s);
        s->kind = GC_SCREEN_MYSTERY;
        s->mystery_type = (int8_t)p->a;
        s->mystery_tier = (int8_t)p->b;
        gc_mystery_setup_screen(run);
        return false;
    }
    case RP_GAME_OVER: {
        gc_screen *s = &run->screen;
        memset(s, 0, sizeof *s);
        bool clear = p->a || (run->run.mode == GAME_MODES_CLASSIC && run->wave > 200);
        if (run->terminal_result) clear = run->terminal_result == 2;
        if (run->run.mode == GAME_MODES_CLASSIC && !gc_classic_finish(run,clear,NULL)) gc_stub(run,GC_STUB_REWARD_INPUT);
        s->kind = clear ? GC_SCREEN_CLEAR : GC_SCREEN_GAME_OVER;
        if (run->in_battle) gc_battle_end_cleanup(run);
        run->finished = true;
        return false;
    }
    case RP_CATCH:
        return gc_catch_start(run, p);
    case RP_ME_POST:
        gc_mystery_post(run, p);
        return true;
    default:
        (void)in;
        return true;
    }
}

bool gc_phase_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    *ok = false;
    switch (p->type) {
    case RP_COMMAND:
        if (c->kind != GC_CH_FIGHT && c->kind != GC_CH_POKEMON && c->kind != GC_CH_BALL && c->kind != GC_CH_RUN) {
            *why = "not a command";
            return false;
        }
        if (!gc_battle_player_command(run, c, why)) return false;
        *ok = true;
        return true;
    case RP_SWITCH: {
        if (c->kind != GC_CH_PICK) { *why = "pick a Pokemon"; return false; }
        const gc_screen *s = &run->screen;
        if (c->a == -1 ? !s->can_skip : (c->a < 0 || c->a >= run->n_party || !s->pick_ok[c->a])) {
            *why = "partyUiHandler:noEnergy";
            return false;
        }
        *ok = true;
        if (p->a) {
            /* game-core's own prompt: straight onto the field, then the turn goes on as usual */
            pr_pokemon *old = run->bt.field[0];
            if (old) old->on_field = false;
            gc_mon *m = &run->party[c->a];
            pr_reset_summon_data(&m->b);
            pr_reset_turn_data(&m->b);
            m->b.on_field = true;
            m->b.switch_out_status = false;
            run->bt.field[0] = &m->b;
            /* the party order: the new lead takes the old one's slot */
            int k_new = gc_battle_slot_of(run, c->a), k_old = 0;
            int8_t t = run->party_order[k_old]; run->party_order[k_old] = run->party_order[k_new]; run->party_order[k_new] = t;
            for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
            gc_event *ev = gc_ev_new(run, GC_EV_SUMMON);
            if (ev) { ev->a = 0; ev->b = c->a; }
            gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon(run, "pokemonName", 0, c->a));
            return true;
        }
        after_stop(run, gc_battle_run(run, c->a));
        return true;
    }
    case RP_LEARN_MOVE: {
        if (c->kind != GC_CH_FORGET) { *why = "forget a move or not"; return false; }
        gc_mon *m = pm(run, p->a);
        if (c->a < -1 || c->a >= PR_N_MOVES(&m->b)) { *why = "no such move slot"; return false; }
        *ok = true;
        if (c->a == -1) {
            gc_ev_msg(run, "battle:learnMoveNotLearned", 2, gc_m_mon(run, "pokemonName", 0, p->a), pr_arg_move("moveName", p->b));
            return true;
        }
        gc_ev_msg(run, "battle:countdownPoof", 0);
        gc_ev_msg(run, "battle:learnMoveForgetSuccess", 2, gc_m_mon(run, "pokemonName", 0, p->a), pr_arg_move("moveName", m->b.moves[c->a]));
        gc_ev_msg(run, "battle:learnMoveAnd", 0);
        learn(run, p->a, p->b, c->a);
        return true;
    }
    case RP_EVOLUTION:
        if (c->kind == GC_CH_EVOLVE) { *ok = true; evolve(run, p); return true; }
        if (c->kind == GC_CH_CANCEL) {
            *ok = true;
            gc_ev_msg(run, "menu:stoppedEvolving", 1, gc_m_mon(run, "pokemonName", 0, p->a));
            if (c->a == 1) {
                pm(run, p->a)->pause_evolutions = true;
                gc_ev_msg(run, "menu:evolutionsPaused", 1, gc_m_mon(run, "pokemonName", 0, p->a));
            }
            return true;
        }
        *why = "evolve or cancel";
        return false;
    case RP_CHECK_SWITCH:
        if (c->kind != GC_CH_CONFIRM || (c->a != 0 && c->a != 1)) { *why = "answer yes or no"; return false; }
        *ok = true;
        /* yes: SwitchPhase(INITIAL_SWITCH) opens the party screen (a stop at PH_SWITCH), no: the encounter goes on */
        after_stop(run, gc_battle_answer_switch(run, c->a == 1));
        return true;
    case RP_SELECT_MODIFIER:
        return select_modifier_choose(run, p, c, why, ok);
    case RP_SELECT_BIOME:
        if (c->kind != GC_CH_BIOME || c->a < 0 || c->a >= run->screen.n_biomes) { *why = "no such biome"; return false; }
        *ok = true;
        set_next_biome(run, run->screen.biomes[c->a]);
        return true;
    case RP_MYSTERY:
        if (c->kind != GC_CH_MYSTERY) { *why = "choose a mystery encounter action"; return false; }
        return gc_mystery_execute(run, c->a, c->b, c->c, why, ok);
    case RP_CATCH:
        return gc_catch_choose(run, p, c, why, ok);
    default:
        *why = "nothing to answer";
        return false;
    }
}
