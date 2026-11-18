/* D'Arby the Gambler (Soul Poker): lorenzo's own mystery encounter for Bizarre runs (his spec, 2026-11-17; custom policy, no upstream).
 * This file holds the hand's rules, which the spec fixes completely:
 *   Score(A -> B) = max over A's types t of the product over B's types d of TypeChart[t][d] ("maximum STAB effectiveness");
 *   the higher score wins; equal scores: the higher base Speed wins; equal base Speed too: a coin flip.
 *   D'Arby's fair card is one of his six, drawn before the player picks. After the pick: if the fair card wins (after the Speed tie-break and the
 *   coin), he plays fair. If it loses and a card on his bench beats the player's pick strictly (score, then base Speed; a coin flip is no win),
 *   he cheats with 75% and swaps to the card with the largest score margin (ties: the highest base Speed), else he plays fair.
 * The encounter flow (wagers, deck, reveal, accusation, rewards) is built on these. */
#include "gc/achv.h"
#include "internal.h"
#include <math.h>
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

/* ---------------------------------------------------------------- the encounter (lorenzo's answers 2026-11-17)
 * The first screen: 0 Standard (one soul, picked on its party screen), 1 All-In (three souls), 2 Walk Away (no shop, nothing happens).
 * Then: the remaining souls one by one (fainted Pokemon count), the deal (six cards face up, any species, distinct, at the level of the player's highest
 * Pokemon; the fair one drawn from the six in secret), the player's card (a party member; independent of the souls), the reveal. A win pays at once; a
 * loss offers Accept the Deal or You Cheated! (and only then the lie detector's line). No ordinary shop after any result.
 *   Standard win: 2 Rarer Candy, then two vitamins, the player picking the vitamin and the Pokemon for each.
 *   All-In win: a Big Nugget, then a reward screen of Master-tier items and one of Rogue-tier items, as many as the shop would offer now.
 *   A loss: the souls leave the party with their held items. A false accusation also takes all of the player's money. */
#define DARBY_FAINTED_CARD_OK 1   /* a fainted Pokemon may be played as the card (asked 2026-11-17, unanswered; one switch) */

static const char *const DARBY_X[17] = {"0x", "0.25x", "0.5x", "", "1x", "", "", "", "2x", "", "", "", "", "", "", "", "4x"};
static const char *const DARBY_LINES[] = {"mysteryEncounters/darby:line.1", "mysteryEncounters/darby:line.2"};   /* his pool of ambiguous lines */

static pr_msg_arg darby_species_arg(const char *name, const gc_darby_card *c)
{
    pr_msg_arg a = pr_arg_int(name, 0);
    a.kind = PR_ARG_POKEMON_NAME;
    a.v = pr_msg_pokemon(c->species, c->form, 0);
    return a;
}

/* The deal, the reveal, the lie detector and result lines wait for A, so a fast game speed cannot skip what happened and the cards stay up
 * long enough to see (lorenzo 2026-11-18). */
static void darby_ev_prompt(gc_event *ev)
{
    if (ev) ev->prompt = true;
}

static pr_msg_arg darby_x_arg(const char *name, int score4)
{
    pr_msg_arg a = pr_arg_int(name, 0);
    a.kind = PR_ARG_STR;
    a.s = score4 >= 0 && score4 <= 16 && DARBY_X[score4][0] ? DARBY_X[score4] : "?";
    return a;
}

static gc_darby_card mon_card(const gc_mon *m)
{
    gc_darby_card c = {m->b.species, m->b.form};
    return c;
}

static bool is_soul(const gc_run *run, int idx)
{
    const gc_internal *in = gc_int((gc_run *)run);
    for (int i = 0; i < in->darby.n_souls; i++) if (in->darby.souls[i] == idx) return true;
    return false;
}

static int souls_needed(const gc_run *run) { return gc_int((gc_run *)run)->darby.wager == GC_DARBY_ALL_IN ? 3 : 1; }

/* the vitamin `stat` can still go on member idx: not at its maximum (me_give_vitamin leaves a full stack alone) */
static bool vitamin_ok(const gc_run *run, int stat, int idx)
{
    if (idx < 0 || idx >= run->n_party || stat < 0 || stat > 5) return false;
    const gc_mon *m = &run->party[idx];
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!strcmp(it->key, "BASE_STAT_BOOSTER") && it->arg == stat && it->pokemon_id == m->b.id) return it->stack + it->virtual_stack < m->b.ivs[stat];
    }
    return true;
}

bool gc_darby_pick_ok(const gc_run *run, int option, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    switch (run->screen.me_stage) {
    case GC_ME_STAGE_FIRST: return option == GC_DARBY_STANDARD || option == GC_DARBY_ALL_IN;   /* any Pokemon, fainted ones too */
    case GC_ME_STAGE_DARBY_SOUL: return option == 0 && !is_soul(run, idx);
    case GC_ME_STAGE_DARBY_CARD: return option == 0 && (DARBY_FAINTED_CARD_OK || run->party[idx].b.hp > 0);
    case GC_ME_STAGE_DARBY_VITAMIN: return vitamin_ok(run, option, idx);
    default: return false;
    }
}

int gc_darby_detector(const gc_run *run, int *ability, int *move)
{
    static const int abilities[] = {ABILITY_ID_KEEN_EYE, ABILITY_ID_INFILTRATOR, ABILITY_ID_FRISK, ABILITY_ID_FOREWARN, ABILITY_ID_ANTICIPATION,
                                    ABILITY_ID_COMPOUND_EYES, ABILITY_ID_MAGICIAN, ABILITY_ID_PICKPOCKET};
    static const int moves[] = {MOVE_ID_FORESIGHT, MOVE_ID_ODOR_SLEUTH, MOVE_ID_MIRACLE_EYE, MOVE_ID_MIND_READER, MOVE_ID_LOCK_ON, MOVE_ID_TRICK,
                                MOVE_ID_SWITCHEROO, MOVE_ID_ROLE_PLAY};
    *ability = *move = -1;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        const gc_mon *m = &run->party[idx];
        if (m->b.hp <= 0) continue;   /* "any conscious party Pokemon" */
        const int own = pr_get_ability(&m->b, false);
        for (size_t j = 0; j < sizeof abilities / sizeof abilities[0]; j++)
            if (own == abilities[j] || (m->b.passive_flag && m->b.passive == abilities[j])) { *ability = abilities[j]; return idx; }
        for (int i = 0; i < 4; i++)
            for (size_t j = 0; j < sizeof moves / sizeof moves[0]; j++)
                if (m->b.moves[i] == moves[j]) { *move = moves[j]; return idx; }
    }
    return -1;
}

void gc_darby_deal(gc_run *run)
{
    gc_internal *in = gc_int(run);
    int level = 1;
    for (int i = 0; i < run->n_party; i++) if (run->party[i].b.level > level) level = run->party[i].b.level;
    in->darby.level = (int16_t)level;
    for (int k = 0; k < GC_DARBY_DECK; k++) {
        for (;;) {
            const int id = pr_species_table[pr_rng_seed_int(&in->darby.r, PR_SPECIES_COUNT, 0)].id;
            bool dup = false;
            for (int j = 0; j < k; j++) dup |= in->darby.deck[j].species == id;
            if (!dup && gc_form(id, 0)) { in->darby.deck[k].species = (uint16_t)id; in->darby.deck[k].form = 0; break; }
        }
    }
    in->darby.fair = (int8_t)pr_rng_seed_int(&in->darby.r, GC_DARBY_DECK, 0);
    in->darby.dealt = true;
}

static void open_stage(gc_run *run, int stage, int n_options)
{
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_MYSTERY;
    s->mystery_type = GC_ME_DARBY;
    s->mystery_tier = GC_ME_ULTRA;
    s->me_stage = (uint8_t)stage;
    s->n_options = n_options;
    for (int i = 0; i < n_options; i++) s->option_ok[i] = true;
    for (int i = 0; i < 3; i++) s->mc_trainer[i] = -1;
}

static void push_stage(gc_run *run, int stage) { gc_rq_push(run, gc_rp(RP_DARBY, stage, 0, 0)); }

static void darby_end(gc_run *run)
{
    darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:outro", 0));
    gc_me_leave(run);   /* no ordinary shop after any result */
}

/* the souls leave the party with what they held (the highest storage index first, so the others keep theirs) */
static void take_souls(gc_run *run)
{
    gc_internal *in = gc_int(run);
    int8_t s[3];
    const int n = in->darby.n_souls;
    memcpy(s, in->darby.souls, sizeof s);
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (s[j] > s[i]) { const int8_t t = s[i]; s[i] = s[j]; s[j] = t; }
    for (int i = 0; i < n; i++) {
        if (s[i] < 0 || s[i] >= run->n_party) continue;
        gc_mon *m = &run->party[s[i]];
        m->b.hp = 0;   /* it faints */
        darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.soulTaken", 1, pr_arg_pokemon_name(&run->bt, "pokemonName", &m->b)));
        gc_party_remove(run, s[i]);
    }
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);
}

static void darby_win(gc_run *run)
{
    gc_internal *in = gc_int(run);
    gc_achv_darby_won(run->sys);   /* either wager, a won hand or a caught cheat */
    darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.win", 0));
    gc_rphase p = gc_rp(RP_DARBY, GC_DARBY_STAGE_GIVE, 0, 0);
    if (in->darby.wager == GC_DARBY_STANDARD) {
        p.key = "RARER_CANDY";
        gc_rq_push(run, p);
        gc_rq_push(run, p);
        in->darby.vitamins_left = 2;
        push_stage(run, GC_DARBY_STAGE_VITAMIN);
        push_stage(run, GC_DARBY_STAGE_VITAMIN);
    } else {
        p.key = "BIG_NUGGET";
        gc_rq_push(run, p);
        push_stage(run, GC_DARBY_STAGE_SHOP);
        push_stage(run, GC_DARBY_STAGE_SHOP);
    }
    darby_end(run);
}

static void darby_lose(gc_run *run, bool false_call)
{
    take_souls(run);
    if (false_call) {
        const double before = run->money;
        run->money = 0;
        gc_ev_money(run, -before);
        darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.moneyGone", 0));
    }
    darby_end(run);
}

void gc_darby_execute(gc_run *run, int option, int member)
{
    gc_internal *in = gc_int(run);
    memset(&in->darby, 0, sizeof in->darby);
    in->darby.fair = in->darby.shown = in->darby.card = in->darby.detector = -1;
    for (int i = 0; i < 3; i++) in->darby.souls[i] = -1;
    if (option == GC_DARBY_WALK_AWAY) { gc_me_leave(run); return; }
    in->darby.wager = (uint8_t)option;
    in->darby.r = gc_me_option_rng(run);
    if (member >= 0 && member < run->n_party) in->darby.souls[in->darby.n_souls++] = (int8_t)member;
    for (int i = in->darby.n_souls; i < souls_needed(run); i++) push_stage(run, GC_DARBY_STAGE_SOUL);
    push_stage(run, GC_DARBY_STAGE_CARD);
}

bool gc_darby_stage_start(gc_run *run, const gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    gc_screen *s = &run->screen;
    switch (p->a) {
    case GC_DARBY_STAGE_SOUL:
        open_stage(run, GC_ME_STAGE_DARBY_SOUL, 1);
        s->option_pick[0] = GC_PICK_MEMBER;
        s->me_args[s->n_me_args++] = pr_arg_int("soulsLeft", souls_needed(run) - in->darby.n_souls);
        return false;
    case GC_DARBY_STAGE_CARD: {
        if (!in->darby.dealt) gc_darby_deal(run);
        open_stage(run, GC_ME_STAGE_DARBY_CARD, 1);
        s->option_pick[0] = GC_PICK_MEMBER;
        static const char *const names[GC_DARBY_DECK] = {"card1", "card2", "card3", "card4", "card5", "card6"};
        for (int k = 0; k < GC_DARBY_DECK; k++) s->me_args[s->n_me_args++] = darby_species_arg(names[k], &in->darby.deck[k]);
        s->me_args[s->n_me_args++] = pr_arg_int("cardLevel", in->darby.level);
        return false;
    }
    case GC_DARBY_STAGE_ACCUSE:
        open_stage(run, GC_ME_STAGE_DARBY_ACCUSE, 2);
        return false;
    case GC_DARBY_STAGE_VITAMIN:
        open_stage(run, GC_ME_STAGE_DARBY_VITAMIN, 6);
        for (int i = 0; i < 6; i++) {
            s->option_pick[i] = GC_PICK_MEMBER;
            bool any = false;
            for (int k = 0; k < run->n_party; k++) any |= vitamin_ok(run, i, k);
            s->option_ok[i] = any;
        }
        s->me_args[s->n_me_args++] = pr_arg_int("vitaminsLeft", in->darby.vitamins_left);
        {
            bool any = false;
            for (int i = 0; i < 6; i++) any |= s->option_ok[i];
            if (!any) { memset(s, 0, sizeof *s); in->darby.vitamins_left--; return true; }   /* every vitamin is at its maximum on everyone: nothing to give */
        }
        return false;
    case GC_DARBY_STAGE_GIVE: {
        /* a free reward applied as the reward screen applies it (cost -1): Rarer Candy levels every member up, the Big Nugget pays */
        gc_offer o;
        memset(&o, 0, sizeof o);
        o.key = p->key;
        o.arg = -1;
        o.cost = -1;
        const char *why = NULL;
        gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", p->key));
        if (gc_reward_apply(run, &o, -1, -1, &why) != GC_APPLY_DONE) gc_stub(run, GC_STUB_REWARD_APPLY);
        return true;
    }
    case GC_DARBY_STAGE_SHOP: {
        /* the first reward screen is Master-tier items, the second Rogue-tier (lorenzo 2026-11-18: two Master screens were too strong), as many as the shop offers now (getModifierCount) */
        pr_reward_custom *c = &in->reward.me_custom;
        memset(c, 0, sizeof *c);
        int n = pr_reward_get_modifier_count(gc_reward_ctx(run), NULL);
        if (n > PR_REWARD_MAX_OPTIONS) n = PR_REWARD_MAX_OPTIONS;
        if (n > GC_MAX_OPTIONS) n = GC_MAX_OPTIONS;
        const int tier = in->darby.shops == 0 ? MODIFIER_TIER_MASTER : MODIFIER_TIER_ROGUE;
        for (int i = 0; i < n; i++) c->tiers[c->n_tiers++] = tier;
        c->fill_remaining = false;
        c->reroll_multiplier = -1;   /* no Reroll on a Master screen (lorenzo 2026-11-18: the button turned it into Lure and Berries) */
        c->allow_luck_upgrades = true;
        in->reward.me_round = in->darby.shops++;   /* the second screen is a new roll, not the first again */
        gc_rq_unshift(run, gc_rp(RP_SELECT_MODIFIER, 2, 0, 0));
        return true;
    }
    default:
        return true;
    }
}

/* the reveal after the player's card: both flips with their multipliers, his line, the tie-break, the result */
static void reveal(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const gc_darby_card player = mon_card(&run->party[in->darby.card]);
    bool cheated;
    in->darby.shown = (int8_t)gc_darby_reveal(in->darby.deck, in->darby.fair, &player, &in->darby.r, &cheated);
    in->darby.cheated = cheated;
    const gc_darby_card *his = &in->darby.deck[in->darby.shown];
    darby_ev_prompt(gc_ev_msg(run, DARBY_LINES[pr_rng_seed_int(&in->darby.r, (int)(sizeof DARBY_LINES / sizeof DARBY_LINES[0]), 0)], 0));
    darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:reveal.player", 2, pr_arg_pokemon_name(&run->bt, "pokemonName", &run->party[in->darby.card].b),
              darby_x_arg("multiplier", gc_darby_score4(&player, his))));
    darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:reveal.darby", 2, darby_species_arg("pokemonName", his), darby_x_arg("multiplier", gc_darby_score4(his, &player))));
    if (gc_darby_score4(&player, his) == gc_darby_score4(his, &player)) {
        darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:reveal.speedTie", 0));
        if (gc_darby_base_speed(&player) == gc_darby_base_speed(his)) darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:reveal.coin", 0));
    }
    in->darby.won = gc_darby_player_wins(&player, his, &in->darby.r);
    if (in->darby.won) { darby_win(run); return; }
    darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.handLost", 0));
    /* the lie detector, only after a loss */
    int ability, move;
    in->darby.detector = (int8_t)gc_darby_detector(run, &ability, &move);
    if (in->darby.detector >= 0) {
        const pr_msg_arg who = pr_arg_pokemon_name(&run->bt, "pokemonName", &run->party[in->darby.detector].b);
        if (in->darby.cheated)
            darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:detector.caught", 2, who, ability >= 0 ? pr_arg_ability("source", ability) : pr_arg_move("source", move | PR_MSG_MOVE_BARE)));
        else
            darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:detector.clean", 1, who));
    }
    push_stage(run, GC_DARBY_STAGE_ACCUSE);
}

bool gc_darby_stage_choose(gc_run *run, const gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    gc_internal *in = gc_int(run);
    if (c->kind != GC_CH_MYSTERY) { *why = "choose a mystery encounter action"; return false; }
    if (c->a < 0 || c->a >= run->screen.n_options || !run->screen.option_ok[c->a]) { *why = "option not allowed"; return false; }
    const bool picks = p->a == GC_DARBY_STAGE_SOUL || p->a == GC_DARBY_STAGE_CARD || p->a == GC_DARBY_STAGE_VITAMIN;
    if (picks && !gc_darby_pick_ok(run, c->a, c->b)) { *why = "no such Pokemon"; return false; }
    *ok = true;
    const int b = c->b;
    memset(&run->screen, 0, sizeof run->screen);
    switch (p->a) {
    case GC_DARBY_STAGE_SOUL:
        in->darby.souls[in->darby.n_souls++] = (int8_t)b;
        return true;
    case GC_DARBY_STAGE_CARD:
        in->darby.card = (int8_t)b;
        reveal(run);
        return true;
    case GC_DARBY_STAGE_ACCUSE:
        if (c->a == 0) { darby_lose(run, false); return true; }
        if (in->darby.cheated) {
            darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.caught", 0));
            darby_win(run);
        } else {
            darby_ev_prompt(gc_ev_msg(run, "mysteryEncounters/darby:result.falseCall", 0));
            darby_lose(run, true);
        }
        return true;
    case GC_DARBY_STAGE_VITAMIN:
        gc_me_give_vitamin(run, b, c->a);
        in->darby.vitamins_left--;
        return true;
    default:
        *why = "nothing to answer";
        *ok = false;
        return false;
    }
}

bool gc_darby_table(const gc_run *run, int *party_index, int *species, int *form)
{
    const gc_internal *in = gc_int((gc_run *)run);
    if (!in->darby.dealt || in->darby.shown < 0 || in->darby.shown >= GC_DARBY_DECK || in->darby.card < 0) return false;
    *party_index = in->darby.card;
    *species = in->darby.deck[in->darby.shown].species;
    *form = in->darby.deck[in->darby.shown].form;
    return true;
}
