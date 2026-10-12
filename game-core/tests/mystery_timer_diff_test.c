/* Part Timer against upstream at 8555c08: replays the "-timer" trace of tests/mystery-actions.producer.test.ts (the real
 * encounter's onPreOptionPhase with selectPokemonForOption captured, the party screen's callback outside any seed offset,
 * onOptionPhase in executeWithSeedOffset(.., * 500) with the dialogue it shows recorded, the rewards phase's
 * doEncounterRewardsAndContinue with the PartyExpPhase really started, the shop it queues and EggLapsePhase.start())
 * through game-core's gc_mystery_setup_screen / gc_mystery_execute and the reward screen it opens.
 *
 * Per case, compared with what upstream did: the starting stats (with every move's PP used, PP Ups and maximum override)
 * and the level cap; which options the app offers (the party screen filter's mask, the third option's requirement and the
 * Pokemon it picks as primary); the run RNG (nothing is drawn) and money after the option (Amulet Coin counted, clamped);
 * the money multiplier, exactly; every move of every Pokemon's PP used afterwards; the messages in order; the phases queued
 * in order and the one experience phase with its exact amount; the shop's settings (fillRemaining, rerollMultiplier, no
 * guaranteed options, luck upgrades) and the options the reward screen shows (item, argument, tier, upgrade count, in order)
 * with the run RNG after it; and the eggs' wave counters. The experience phases are taken out of the queue before the
 * screen opens (upstream's shop is computed without them applied).
 * Usage: mystery_timer_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(char want)
{
    char c;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

#define MAX_MSG 8
static char msgs[MAX_MSG][64];
static int n_msgs;
static void collect(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || n_msgs >= MAX_MSG) continue;
        const char *k = strrchr(e.msg.key, ':');
        k = k ? k + 1 : e.msg.key;
        if (!strcmp(k, "receiveMoney")) snprintf(msgs[n_msgs++], sizeof msgs[0], "receiveMoney:%d", (int)e.msg.args[0].v);
        else snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", k);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_timer_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, shown = 0, by_option[3] = {0}, disabled = 0, good = 0, bad_pay = 0, setup_checked = 0, exp_phases = 0,
        options_checked = 0, pp_checked = 0, msgs_checked = 0, filters_checked = 0, clamped = 0, amulet_runs = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        const int i = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money_start = real();
        const int pick = (int)integer(), runs = (int)integer();
        struct { int species, form, level, nature, ivs[6], pokerus, ability, passive, hp, max, stats[6], n_moves, moves[4], pp_used[4], pp_up[4], override[4]; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].pokerus = (int)integer(); setup[m].ability = (int)integer(); setup[m].passive = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
            setup[m].n_moves = (int)integer();
            for (int k = 0; k < setup[m].n_moves; k++) {
                setup[m].moves[k] = (int)integer(); setup[m].pp_used[k] = (int)integer(); setup[m].pp_up[k] = (int)integer(); setup[m].override[k] = (int)integer();
            }
        }
        tag('I');
        const int share = (int)integer(), balance = (int)integer(), oval = (int)integer(), amulet = (int)integer(), n_egg_items = (int)integer();
        int egg_item[12][3];
        for (int k = 0; k < n_egg_items; k++) for (int j = 0; j < 3; j++) egg_item[k][j] = (int)integer();
        tag('G');
        const int n_eggs = (int)integer();
        int eggs[8];
        for (int k = 0; k < n_eggs; k++) eggs[k] = (int)integer();
        tag('A');
        const int meets[3] = {(int)integer(), (int)integer(), (int)integer()};
        const int mask = (int)integer(), primary = (int)integer();
        const double max_exp_level = real();
        pr_rng before = {0}, after_pre = {0}, after_pick = {0}, after_option = {0}, after_screen = {0};
        double multiplier = 0, money_after = 0;
        int leave_arg = 0, n_queued = 0, n_up_msgs = 0, n_exp = 0, count = 0, n_opts = 0, n_up_eggs = 0, up_eggs[8];
        char queued[8][40], up_msgs[MAX_MSG][64], t_args[6][24];
        int up_pp[6][4];
        struct { int kind, index; double exp; } up_exp[6];
        struct { char key[40]; int arg, tier, upgrade; } want[16];
        if (runs) {
            tag('R'); before = rng_vals();
            tag('B'); after_pre = rng_vals();
            tag('Y'); after_pick = rng_vals(); multiplier = real();
            tag('W');
            for (int m = 0; m < n_mons; m++) for (int k = 0; k < setup[m].n_moves; k++) up_pp[m][k] = (int)integer();
            tag('Q'); after_option = rng_vals(); money_after = real(); leave_arg = (int)integer();
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('X');
            n_queued = (int)integer();
            for (int k = 0; k < n_queued; k++) word(queued[k], sizeof queued[k]);
            tag('T');
            for (int k = 0; k < 6; k++) word(t_args[k], sizeof t_args[k]);
            tag('F');
            n_exp = (int)integer();
            for (int k = 0; k < n_exp; k++) { up_exp[k].kind = (int)integer(); up_exp[k].index = (int)integer(); up_exp[k].exp = real(); }
            tag('O');
            count = (int)integer(); n_opts = (int)integer();
            for (int k = 0; k < n_opts; k++) {
                word(want[k].key, sizeof want[k].key);
                want[k].arg = (int)integer(); want[k].tier = (int)integer(); want[k].upgrade = (int)integer();
            }
            tag('S'); after_screen = rng_vals();
            tag('H');
            char w[24];
            for (int k = 0; k < n_eggs; k++) { word(w, sizeof w); up_eggs[n_up_eggs++] = atoi(w); }
            if (!n_eggs) word(w, sizeof w);   /* "-" */
        }
        tag('E');
        (void)after_pre; (void)count; (void)n_up_eggs;

        int st[6], at[6];
        for (int m = 0; m < n_mons; m++) { st[m] = (m + i % n_mons) % n_mons; at[st[m]] = m; }
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, run_seed, 42);
        gc_starter team[1];
        memset(team, 0, sizeof team);
        team[0].species = SPECIES_ID_BULBASAUR;
        team[0].tera_type = -1;
        for (int k = 0; k < 6; k++) team[0].ivs[k] = 15;
        const char *why;
        if (!gc_run_set_starters(&run, team, 1, &why)) { fprintf(stderr, "starters: %s\n", why); return 2; }
        gc_system_init_defaults(&sys);
        for (int k = 0; k < n_eggs; k++) gc_system_add_egg(&sys, (uint32_t)(k + 1), eggs[k], 0, SPECIES_ID_BULBASAUR);
        run.sys = &sys;
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            memset(mon->b.pp_up, 0, sizeof mon->b.pp_up);
            for (int k = 0; k < 4; k++) mon->b.max_pp_override[k] = -1;
            for (int k = 0; k < setup[m].n_moves; k++) {
                mon->b.moves[k] = (uint16_t)setup[m].moves[k];
                mon->b.pp_used[k] = (uint8_t)setup[m].pp_used[k];
                mon->b.pp_up[k] = (uint8_t)setup[m].pp_up[k];
                mon->b.max_pp_override[k] = (int16_t)setup[m].override[k];
            }
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->b.on_field = false;
            mon->pokerus = setup[m].pokerus != 0;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
            mon->b.passive = (uint16_t)setup[m].passive;
            mon->b.passive_flag = setup[m].passive != 0;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        if (share) gc_item_add(&run, "EXP_SHARE", 0, 0, share);
        if (balance) gc_item_add(&run, "EXP_BALANCE", 0, 0, balance);
        if (oval) gc_item_add(&run, "OVAL_CHARM", 0, 0, oval);
        if (amulet) gc_item_add(&run, "AMULET_COIN", 0, 0, amulet);
        for (int k = 0; k < n_egg_items; k++) gc_item_add(&run, egg_item[k][1] == 40 ? "LUCKY_EGG" : "GOLDEN_EGG", 0, run.party[st[egg_item[k][0]]].b.id, egg_item[k][2]);
        gc_items_to_battle(&run);
        int bad = 0;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d pick %d): ", i, option, wave, pick); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = true;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            setup_checked++;
            mon->b.hp = setup[m].hp;
        }
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.money = money_start;
        run.battle_type = BATTLE_TYPE_WILD;
        CHECK(pr_run_get_max_exp_level(&run.run, wave, false) == max_exp_level, "level cap %.0f, upstream %.0f", pr_run_get_max_exp_level(&run.run, wave, false), max_exp_level);
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_PART_TIMER;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_PART_TIMER;
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        /* the first two options have no requirement of their own: the app offers them when the party screen would let a Pokemon through */
        CHECK(meets[0] == 1 && meets[1] == 1, "upstream's options 0/1 need nothing");
        CHECK(run.screen.option_ok[0] == (mask != 0) && run.screen.option_ok[1] == (mask != 0), "options 0/1 are %d/%d, the filter lets %d Pokemon through", run.screen.option_ok[0], run.screen.option_ok[1], mask);
        CHECK(run.screen.option_ok[2] == (meets[2] != 0), "option 3 is %s, upstream %s", run.screen.option_ok[2] ? "enabled" : "disabled", meets[2] ? "enabled" : "disabled");
        for (int m = 0; m < n_mons; m++) {
            for (int o = 0; o < 2; o++) CHECK(gc_mystery_pick_ok(&run, GC_ME_PART_TIMER, o, st[m]) == (((mask >> m) & 1) != 0), "party screen filter for Pokemon %d, option %d", m, o);
            filters_checked += 2;
        }
        {
            const int p = gc_mystery_primary_pokemon(&run, GC_ME_PART_TIMER, 2);
            CHECK(meets[2] ? p == st[primary] : p == -1, "primary Pokemon %d, upstream %d", p, meets[2] ? st[primary] : -1);
            filters_checked++;
        }
        if (!runs) {
            CHECK(!run.screen.option_ok[option], "option %d is enabled in C but upstream could not run it", option);
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        collect();
        n_msgs = 0;
        {
            const gc_mon *picked = &run.party[st[pick]];
            CHECK(gc_part_timer_multiplier(picked, option) == multiplier, "money multiplier %.17g, upstream %.17g", gc_part_timer_multiplier(picked, option), multiplier);
            clamped += option < 2 && (multiplier == 1 || multiplier == 4);
        }
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, st[pick], 0, &w2, &ok);
        collect();
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&before, &after_pre) && same_rng(&before, &after_pick) && same_rng(&before, &after_option), "upstream's RNG moved");
        CHECK(same_rng(&run.run.rng, &before), "the run RNG moved during the option");
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        CHECK(leave_arg == 0, "upstream left with addHealPhase %d", leave_arg);
        for (int m = 0; m < n_mons; m++)
            for (int k = 0; k < setup[m].n_moves; k++) {
                CHECK(run.party[st[m]].b.pp_used[k] == up_pp[m][k], "PP used of Pokemon %d move %d is %d, upstream %d", m, k, run.party[st[m]].b.pp_used[k], up_pp[m][k]);
                pp_checked++;
            }
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) { CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]); msgs_checked++; }
        good += n_up_msgs > 0 && !strcmp(up_msgs[0], "jobCompleteGood");
        bad_pay += n_up_msgs > 0 && !strcmp(up_msgs[0], "jobCompleteBad");

        /* the queue: the experience phase first (upstream's PartyExpPhase, unshifted ahead of the rest), then the shop */
        CHECK(n_queued == 3 && !strcmp(queued[0], "PartyExpPhase") && !strcmp(queued[1], "SelectModifierPhase") && !strcmp(queued[2], "+PostMysteryEncounterPhase"),
              "upstream queued %d phases (%s ...)", n_queued, queued[0]);
        gc_rphase kept[16];
        int n_kept = 0, seen_exp = 0;
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int pos = 0;
            while (gc_rq_next(&copy, &p)) {
                if (p.type == RP_EXP) {
                    CHECK(n_kept == 0, "an experience phase after phase %d", kept[0].type);
                    if (seen_exp < n_exp) {
                        const bool same = up_exp[seen_exp].index == at[p.a] && p.d == up_exp[seen_exp].exp && up_exp[seen_exp].kind == (p.b ? 0 : 1);
                        CHECK(same, "experience phase %d is Pokemon %d for %.17g, upstream Pokemon %d for %.17g", seen_exp, at[p.a], p.d, up_exp[seen_exp].index, up_exp[seen_exp].exp);
                    }
                    seen_exp++;
                } else {
                    if (n_kept == 0) CHECK(p.type == RP_SELECT_MODIFIER && p.a == 2, "the first phase after the experience is %d (a=%d), want the encounter's shop", p.type, (int)p.a);
                    else CHECK(p.type == RP_NEW_BATTLE || p.type == RP_SELECT_BIOME, "queued phase %d at position %d", p.type, pos);
                    if (n_kept < 16) kept[n_kept++] = p;
                }
                pos++;
            }
            CHECK(seen_exp == n_exp, "%d experience phases queued, upstream %d", seen_exp, n_exp);
            exp_phases += seen_exp;
        }

        /* the shop's settings */
        const pr_reward_custom *cu = &gc_int(&run)->reward.me_custom;
        CHECK(cu->n_options == atoi(t_args[4]) && cu->n_funcs == 0 && cu->n_tiers == 0, "shop has %d options/%d funcs/%d tiers, want %s guaranteed options", cu->n_options, cu->n_funcs, cu->n_tiers, t_args[4]);
        CHECK(cu->fill_remaining == (atoi(t_args[2]) != 0), "fillRemaining");
        CHECK(!strcmp(t_args[3], "nan") ? isnan(cu->reroll_multiplier) : cu->reroll_multiplier == atof(t_args[3]), "rerollMultiplier");
        CHECK(cu->allow_luck_upgrades == (atoi(t_args[5]) != 0), "allowLuckUpgrades %d vs %s (undefined = allowed)", cu->allow_luck_upgrades, t_args[5]);

        /* the screen it opens, the experience taken out of the queue */
        gc_rq_reset(&gc_int(&run)->rq);
        for (int k = 0; k < n_kept; k++) gc_rq_push(&run, kept[k]);
        gc_int(&run)->rq.waiting = false;
        memset(&run.screen, 0, sizeof run.screen);
        gc_run_until_screen(&run);
        CHECK(run.screen.kind == GC_SCREEN_REWARD, "screen %d, want the reward screen", run.screen.kind);
        CHECK(run.screen.n_options == n_opts, "%d options, want %d", run.screen.n_options, n_opts);
        for (int k = 0; k < n_opts && k < run.screen.n_options; k++) {
            const gc_offer *o = &run.screen.options[k];
            CHECK(!strcmp(o->key, want[k].key) && o->tier == want[k].tier && o->upgrade_count == want[k].upgrade && o->arg == want[k].arg,
                  "option %d is %s/%d tier %d up %d, want %s/%d tier %d up %d", k, o->key, o->arg, o->tier, o->upgrade_count, want[k].key, want[k].arg, want[k].tier, want[k].upgrade);
            options_checked++;
        }
        CHECK(same_rng(&run.run.rng, &after_screen), "the run RNG after the shop differs");
        CHECK(gc_rq_has(&run, RP_NEW_BATTLE), "the next wave is not queued");
        CHECK(sys.n_eggs == n_eggs, "%d eggs", sys.n_eggs);
        for (int k = 0; k < n_eggs && k < sys.n_eggs; k++) CHECK(sys.eggs[k].hatch_waves == up_eggs[k], "egg %d has %d waves left, upstream %d", k, sys.eggs[k].hatch_waves, up_eggs[k]);
        amulet_runs += amulet != 0;
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Part Timer: %d cases run (options %d/%d/%d; %d paid well, %d badly; %d multipliers clamped at 1 or 4; %d with an Amulet Coin), %d not runnable "
           "(no Pokemon to pick or no charming move), %d starting parties, %d party screen filters and primaries, %d moves' PP, %d messages, %d experience phases, %d shop options compared. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], good, bad_pay, clamped, amulet_runs, disabled, setup_checked, filters_checked, pp_checked, msgs_checked, exp_phases, options_checked, mismatches);
    return mismatches ? 1 : 0;
}
