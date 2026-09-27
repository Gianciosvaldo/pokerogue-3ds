/* Experience curves (src/data/exp.ts). Below level 100 upstream reads its own table, whose results
 * are dumped into gen/out/run_exp.c; from 100 on it uses formulas, ported here. */
#include "pr/run.h"
#include <math.h>

/* getLevelTotalExp(level, growthRate):
 *   if (level < 100) → expLevels (non MEDIUM_FAST: Math.floor(levelExp * 0.325 + MEDIUM_FAST's * 0.675))
 *   ERRATIC (level^4 + level^3 * 2000) / 3500; FAST level^3 * 4 / 5; MEDIUM_FAST level^3;
 *   MEDIUM_SLOW level^3 * 6 / 5 - 15 * level^2 + 100 * level - 140; SLOW level^3 * 5 / 4;
 *   FLUCTUATING level^3 * (level / 2 + 8) * 4 / (100 + level);
 *   non MEDIUM_FAST: Math.floor(ret * 0.325 + getLevelTotalExp(level, MEDIUM_FAST) * 0.675); else Math.floor(ret)
 * A level below 1 reads past the table's start: undefined, NaN once blended; NaN here. */
double pr_run_get_level_total_exp(int level, int rate)
{
    if (rate < 0 || rate >= PR_GROWTH_RATE_COUNT) return NAN;
    if (level < 100) return level >= 1 ? pr_exp_table[rate][level - 1] : NAN;
    double l = level, ret = 0;
    switch (rate) {
    case GROWTH_RATE_ERRATIC: ret = (js_pow(l, 4) + js_pow(l, 3) * 2000) / 3500; break;
    case GROWTH_RATE_FAST: ret = (js_pow(l, 3) * 4) / 5; break;
    case GROWTH_RATE_MEDIUM_FAST: ret = js_pow(l, 3); break;
    case GROWTH_RATE_MEDIUM_SLOW: ret = (js_pow(l, 3) * 6) / 5 - 15 * js_pow(l, 2) + 100 * l - 140; break;
    case GROWTH_RATE_SLOW: ret = (js_pow(l, 3) * 5) / 4; break;
    case GROWTH_RATE_FLUCTUATING: ret = (js_pow(l, 3) * (l / 2 + 8) * 4) / (100 + l); break;
    }
    if (rate != GROWTH_RATE_MEDIUM_FAST)
        return floor(ret * 0.325 + pr_run_get_level_total_exp(level, GROWTH_RATE_MEDIUM_FAST) * 0.675);
    return floor(ret);
}

/* getLevelRelExp(level, growthRate): getLevelTotalExp(level) - getLevelTotalExp(level - 1) */
double pr_run_get_level_rel_exp(int level, int rate)
{
    return pr_run_get_level_total_exp(level, rate) - pr_run_get_level_total_exp(level - 1, rate);
}

/* Pokemon.getExpValue(): (getSpeciesForm().getBaseExp() * level) / 5 + 1 (not floored) */
double pr_run_get_exp_value(int base_exp, int level)
{
    return ((double)base_exp * level) / 5 + 1;
}

/* BattleScene.applyPartyExp(expValue, pokemonDefeated, useWaveIndexMultiplier, participantIds): the experience
 * each party member gets (friendship and Macho Brace, which pokemonDefeated adds, aren't part of it;
 * XP_MULTIPLIER_OVERRIDE, a dev override, isn't modelled):
 *   nonFainted = party.filter(p => p.hp); expPartyMembers = nonFainted.filter(p => p.level < getMaxExpLevel());
 *   if (useWaveIndexMultiplier) expValue = Math.floor((expValue * waveIndex) / 5 + 1);
 *   if (participantIds.size > 0) {
 *     trainer battle (or a trainer-battle ME): expValue = Math.floor(expValue * 1.5);
 *     else in another ME: expValue = Math.floor(expValue * mysteryEncounter.expMultiplier);
 *     for (const partyMember of nonFainted) {
 *       if (!expPartyMembers.includes(partyMember)) continue;
 *       if (!participated && !expShareModifier) { partyMemberExp.push(0); continue; }
 *       let expMultiplier = 0;
 *       if (participated) { expMultiplier += 1 / participantIds.size;
 *         if (participantIds.size > 1 && multipleParticipantExpBonusModifier) expMultiplier += stacks * 0.2; }
 *       else if (expShareModifier) expMultiplier += (stacks * 0.2) / participantIds.size;
 *       if (partyMember.pokerus) expMultiplier *= 1.5;
 *       const pokemonExp = new NumberHolder(expValue * expMultiplier);
 *       applyModifiers(PokemonExpBoosterModifier, true, partyMember, pokemonExp); // each: Math.floor(v * (1 + stacks * boost%))
 *       partyMemberExp.push(Math.floor(pokemonExp.value)); }
 *     if (expBalanceModifier) { medianLevel = Math.floor(totalLevel / expPartyMembers.length);
 *       recipients: level <= medianLevel; splitExp = Math.floor(totalExp / recipients.length);
 *       partyMemberExp[pm] = Phaser.Math.Linear(partyMemberExp[pm], recipient ? splitExp : 0, 0.2 * stacks); }
 *     for each expPartyMembers[pm] with partyMemberExp[pm] truthy: ExpPhase / ShowPartyExpBarPhase(party index, exp) }
 * Returns how many members get experience; out_index / out_exp list them in that order. */
int pr_run_apply_party_exp(const pr_exp_party *pp, double exp_value, bool use_wave_index_multiplier, int wave,
                           int out_index[PR_MAX_EXP_PARTY], double out_exp[PR_MAX_EXP_PARTY])
{
    int exp_members[PR_MAX_EXP_PARTY], n_exp = 0;
    double member_exp[PR_MAX_EXP_PARTY];
    for (int i = 0; i < pp->n_party; i++)
        if (pp->party[i].hp && pp->party[i].level < pp->max_exp_level) exp_members[n_exp++] = i;
    if (use_wave_index_multiplier) exp_value = floor((exp_value * wave) / 5 + 1);
    if (pp->n_participants <= 0) return 0;
    if (pp->trainer_battle) exp_value = floor(exp_value * 1.5);
    else if (pp->me_exp_multiplier == pp->me_exp_multiplier) exp_value = floor(exp_value * pp->me_exp_multiplier);
    for (int k = 0; k < n_exp; k++) {
        const pr_exp_member *m = &pp->party[exp_members[k]];
        if (!m->participated && !pp->exp_share_stacks) { member_exp[k] = 0; continue; }
        double mult = 0;
        if (m->participated) {
            mult += 1.0 / pp->n_participants;
            if (pp->n_participants > 1 && pp->oval_charm_stacks) mult += pp->oval_charm_stacks * 0.2;
        } else {
            mult += (pp->exp_share_stacks * 0.2) / pp->n_participants;
        }
        if (m->pokerus) mult *= 1.5;
        double v = exp_value * mult;
        for (int e = 0; e < pp->n_eggs; e++)
            if (pp->eggs[e].pokemon_id == m->id) v = floor(v * (1 + pp->eggs[e].stacks * (pp->eggs[e].boost_percent * 0.01)));
        member_exp[k] = floor(v);
    }
    if (pp->exp_balance_stacks && n_exp) {
        double total_level = 0, total_exp = 0;
        for (int k = 0; k < n_exp; k++) { total_exp += member_exp[k]; total_level += pp->party[exp_members[k]].level; }
        double median = floor(total_level / n_exp);
        int n_recipients = 0;
        for (int k = 0; k < n_exp; k++) n_recipients += pp->party[exp_members[k]].level <= median;
        double split = floor(total_exp / n_recipients);
        for (int k = 0; k < n_exp; k++) {
            double target = pp->party[exp_members[k]].level <= median ? split : 0;
            member_exp[k] = (target - member_exp[k]) * (0.2 * pp->exp_balance_stacks) + member_exp[k];
        }
    }
    int n = 0;
    for (int k = 0; k < n_exp; k++)
        if (member_exp[k] != 0 && member_exp[k] == member_exp[k]) {
            out_index[n] = exp_members[k];
            out_exp[n++] = member_exp[k];
        }
    return n;
}
