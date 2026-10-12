/* Shady Vitamin Dealer against upstream at 8555c08: replays the "-shady" trace of tests/mystery-actions.producer.test.ts
 * (the real encounter's onPreOptionPhase / the party screen's callback / onOptionPhase / the rewards phase's
 * doEncounterRewardsAndContinue / onPostOptionPhase, each in the seed offset its phase uses) through game-core's
 * gc_mystery_execute, the reward screen it opens and the post-option phase it queues.
 *
 * Per case, compared with what upstream did:
 *  - the party at the start (stats and max HP the C side calculates from the same level, IVs, natures and vitamins)
 *  - what the options offer: each option's meetsRequirements() (money) and each Pokemon the party screens' filters
 *    accept, as the screen's option_ok and gc_mystery_pick_ok
 *  - the run RNG after the party screen's callback and the option (the vitamins' two randSeedInt(6) draw from the run's
 *    own RNG), the money, the number of vitamin entries, and every Pokemon's HP, stats, nature and vitamin stacks
 *  - the phases queued: the heal-only shop (and its kind), then the post-option phase, then the next wave, and that no
 *    experience phase is queued (setEncounterExp in onPostOptionPhase runs after the rewards phase: never given)
 *  - after the post-option phase: every Pokemon again, the run RNG (untouched: it runs on its own seed offset), the
 *    messages in order (the money paid with its amount, the side effects / no bad effects text), the eggs' wave counters
 * Usage: mystery_shady_diff_test <trace>; exit 0 only with 0 mismatches. */
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

/* a Pokemon as the trace prints it after an action: hp, max HP, six stats, nature, custom nature, six vitamin stacks */
typedef struct { int hp, max, stats[6], nature, custom, stacks[6]; } mon_state;
static void read_state(mon_state *m)
{
    m->hp = (int)integer(); m->max = (int)integer();
    for (int k = 0; k < 6; k++) m->stats[k] = (int)integer();
    m->nature = (int)integer(); m->custom = (int)integer();
    for (int k = 0; k < 6; k++) m->stacks[k] = (int)integer();
}
static void c_state(const gc_mon *m, mon_state *out)
{
    out->hp = m->b.hp; out->max = m->b.stats[0];
    for (int k = 0; k < 6; k++) out->stats[k] = m->b.stats[k];
    out->nature = m->b.nature;
    out->custom = m->has_custom_nature ? m->b.nature : -1;
    for (int k = 0; k < 6; k++) {
        out->stacks[k] = 0;
        for (int i = 0; i < run.n_items; i++)
            if (!strcmp(run.items[i].key, "BASE_STAT_BOOSTER") && run.items[i].arg == k && run.items[i].pokemon_id == m->b.id) out->stacks[k] += run.items[i].stack;
    }
}
static bool same_state(const mon_state *a, const mon_state *b) { return !memcmp(a, b, sizeof *a); }
static void print_state(const char *who, const mon_state *m)
{
    fprintf(stderr, "    %s: hp %d/%d stats %d %d %d %d %d %d nature %d custom %d stacks %d %d %d %d %d %d\n", who, m->hp, m->max, m->stats[0], m->stats[1],
            m->stats[2], m->stats[3], m->stats[4], m->stats[5], m->nature, m->custom, m->stacks[0], m->stacks[1], m->stacks[2], m->stacks[3], m->stacks[4], m->stacks[5]);
}

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
        if (!strcmp(k, "paidMoney")) snprintf(msgs[n_msgs++], sizeof msgs[0], "paidMoney:%d", (int)e.msg.args[0].v);
        else snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", k);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_shady_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    static struct { int species, form, level, n_moves, moves[4]; } mons[6];
    int n_mons = 0;
    int cases = 0, mismatches = 0, shown = 0, by_option[3] = {0}, disabled = 0, setup_checked = 0, states_checked = 0, filters_checked = 0, msgs_checked = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') { if (integer() != cases + 0 && 0) return 2; break; }
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons; m++) {
                mons[m].species = (int)integer(); mons[m].form = (int)integer(); mons[m].level = (int)integer();
                mons[m].n_moves = (int)integer();
                if (mons[m].n_moves < 0 || mons[m].n_moves > 4) { fputs("moves\n", stderr); return 2; }
                for (int k = 0; k < mons[m].n_moves; k++) mons[m].moves[k] = (int)integer();
            }
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        const int i = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money = real();
        const int pick = (int)integer(), runs = (int)integer();
        struct { int level, nature, custom, ability, passive, ivs[6], stacks[6], hp, max, stats[6]; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            setup[m].level = (int)integer(); setup[m].nature = (int)integer(); setup[m].custom = (int)integer();
            setup[m].ability = (int)integer(); setup[m].passive = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stacks[k] = (int)integer();
            setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
        }
        tag('G');
        const int n_eggs = (int)integer();
        int eggs[8];
        for (int k = 0; k < n_eggs; k++) eggs[k] = (int)integer();
        tag('A');
        int meets[3], masks[2];
        for (int k = 0; k < 3; k++) meets[k] = (int)integer();
        for (int k = 0; k < 2; k++) masks[k] = (int)integer();

        pr_rng before = {0}, after_pre = {0}, after_pick = {0}, after_option = {0}, after_post = {0};
        double money_pick = 0, money_after = 0;
        int stat_draws[2] = {-1, -1}, owned = 0, leave_arg = 0, exp_set = 0, n_queued = 0, n_up_msgs = 0, shop_fill = 0;
        char shop_args[3][24], queued[8][40], up_msgs[MAX_MSG][64];
        char shop_first[16] = "-";
        mon_state after_opt[6], after_post_state[6];
        bool has_post = false;
        int up_eggs[8], n_up_eggs = 0;
        if (runs) {
            tag('R'); before = rng_vals();
            tag('B'); after_pre = rng_vals();
            tag('Y'); after_pick = rng_vals(); money_pick = real();
            if (option < 2) { stat_draws[0] = (int)integer(); stat_draws[1] = (int)integer(); }
            else { char dash[8]; word(dash, sizeof dash); }
            tag('Q'); after_option = rng_vals(); money_after = real(); owned = (int)integer(); leave_arg = (int)integer();
            tag('W');
            for (int m = 0; m < n_mons; m++) read_state(&after_opt[m]);
            tag('X');
            n_queued = (int)integer();
            for (int k = 0; k < n_queued; k++) word(queued[k], sizeof queued[k]);
            word(shop_args[0], sizeof shop_args[0]); word(shop_args[1], sizeof shop_args[1]);
            shop_fill = (int)integer(); word(shop_args[2], sizeof shop_args[2]);
            (void)shop_first;
            tag('T'); after_post = rng_vals(); exp_set = (int)integer();
            tag('V');
            {
                char first[16];
                word(first, sizeof first);
                if (strcmp(first, "-")) {
                    has_post = true;
                    /* the first token was the first Pokemon's hp: put it back by reading the rest around it */
                    after_post_state[0].hp = atoi(first);
                    after_post_state[0].max = (int)integer();
                    for (int k = 0; k < 6; k++) after_post_state[0].stats[k] = (int)integer();
                    after_post_state[0].nature = (int)integer(); after_post_state[0].custom = (int)integer();
                    for (int k = 0; k < 6; k++) after_post_state[0].stacks[k] = (int)integer();
                    for (int m = 1; m < n_mons; m++) read_state(&after_post_state[m]);
                }
            }
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('H');
            {
                char w[24];
                for (int k = 0; k < n_eggs; k++) { word(w, sizeof w); up_eggs[n_up_eggs++] = atoi(w); }
                if (!n_eggs) word(w, sizeof w);   /* "-" */
            }
        }
        tag('E');

        /* the run as the encounter finds it */
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
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            if (m) run.party[m] = run.party[0];
            gc_mon *mon = &run.party[m];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)mons[m].species;
            mon->b.form = (uint8_t)mons[m].form;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.n_moves = (uint8_t)mons[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            for (int k = 0; k < mons[m].n_moves; k++) mon->b.moves[k] = (uint16_t)mons[m].moves[k];
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
            mon->b.passive = (uint16_t)setup[m].passive;
            mon->b.passive_flag = setup[m].passive != 0;
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = setup[m].custom >= 0;
            mon->b.nature = (uint8_t)(setup[m].custom >= 0 ? setup[m].custom : setup[m].nature);
            run.party_order[m] = (int8_t)m;
            for (int k = 0; k < 6; k++) if (setup[m].stacks[k] > 0) gc_item_add(&run, "BASE_STAT_BOOSTER", (int16_t)k, mon->b.id, setup[m].stacks[k]);
        }
        run.n_party = n_mons;
        gc_items_to_battle(&run);
        int bad = 0;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d pick %d): ", i, option, wave, pick); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[m];
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = mon->b.stats[0] == setup[m].max;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            setup_checked++;
            mon->b.hp = setup[m].hp;
        }
        run.wave = wave;
        run.biome = BIOME_ID_PLAINS;
        run.money = money;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_SHADY_VITAMIN_DEALER;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_SHADY_VITAMIN_DEALER;
        gc_mystery_setup_screen(&run);

        /* what the options offer */
        for (int k = 0; k < 2; k++) {
            for (int m = 0; m < n_mons; m++) {
                CHECK(gc_mystery_pick_ok(&run, GC_ME_SHADY_VITAMIN_DEALER, k, m) == (((masks[k] >> m) & 1) != 0), "option %d: Pokemon %d is %s by the party screen filter in C, upstream %s",
                      k, m, gc_mystery_pick_ok(&run, GC_ME_SHADY_VITAMIN_DEALER, k, m) ? "accepted" : "refused", ((masks[k] >> m) & 1) ? "accepted" : "refused");
                filters_checked++;
            }
            CHECK(run.screen.option_ok[k] == (meets[k] && masks[k] != 0), "option %d is %s in C (money ok upstream %d, a Pokemon to pick %d)", k,
                  run.screen.option_ok[k] ? "enabled" : "disabled", meets[k], masks[k] != 0);
        }
        CHECK(run.screen.option_ok[2] == (meets[2] != 0), "the leave option is %s", run.screen.option_ok[2] ? "enabled" : "disabled");
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        if (!runs) {
            CHECK(!run.screen.option_ok[option] || (option < 2 && 0), "option %d is enabled in C but upstream could not run it", option);
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        collect();
        n_msgs = 0;
        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, pick, 0, &w2, &ok);
        collect();
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (the two vitamin stats upstream drew: %d %d)", stat_draws[0], stat_draws[1]);
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        int c_owned = 0;
        for (int k = 0; k < run.n_items; k++) c_owned += !strcmp(run.items[k].key, "BASE_STAT_BOOSTER");
        CHECK(c_owned == owned, "%d vitamin entries, want %d", c_owned, owned);
        CHECK(leave_arg == 1, "upstream left with addHealPhase %d", leave_arg);
        for (int m = 0; m < n_mons; m++) {
            mon_state cs;
            c_state(&run.party[m], &cs);
            states_checked++;
            if (!same_state(&cs, &after_opt[m])) { CHECK(0, "Pokemon %d after the option differs", m); if (shown <= 14) { print_state("C", &cs); print_state("upstream", &after_opt[m]); } }
        }
        (void)money_pick; (void)after_pick; (void)after_pre;

        /* the queue upstream's rewards phase left: SelectModifierPhase(0, undefined, {fillRemaining: false, rerollMultiplier: -1}),
         * the PostMysteryEncounterPhase (only the buying options have a post-option phase); no experience phase */
        CHECK(n_queued == 2, "upstream queued %d phases", n_queued);
        CHECK(!strcmp(queued[0], "SelectModifierPhase") && !strcmp(shop_args[0], "0") && !strcmp(shop_args[1], "u") && shop_fill == 0 && !strcmp(shop_args[2], "-1"),
              "upstream's shop is not the heal-only one (%s %s %d %s)", queued[0], shop_args[0], shop_fill, shop_args[2]);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int pos = 0;
            const int want[3] = {RP_SELECT_MODIFIER, RP_ME_POST, RP_NEW_BATTLE};
            bool have_post = option < 2;
            while (gc_rq_next(&copy, &p)) {
                if (pos == 0) CHECK(p.type == RP_SELECT_MODIFIER && p.a == 1, "first queued phase is %d (a=%d), not the heal-only shop", p.type, (int)p.a);
                else if (pos == 1 && have_post) CHECK(p.type == RP_ME_POST && p.a == GC_ME_SHADY_VITAMIN_DEALER && p.b == pick && p.c == option, "second queued phase is %d, not the post-option phase", p.type);
                else if (p.type != RP_SELECT_BIOME) CHECK(p.type == want[2], "queued phase %d at position %d", p.type, pos);
                pos++;
            }
            CHECK(pos >= (have_post ? 3 : 2), "%d phases queued", pos);
            CHECK(!exp_set || option < 2, "experience set without a post-option phase");
            (void)want;
        }

        /* the shop it opens: no rewards of its own */
        pr_rng rng_at_post = run.run.rng;
        {
            gc_rqueue *q = &gc_int(&run)->rq;
            q->waiting = false;
            memset(&run.screen, 0, sizeof run.screen);
            gc_run_until_screen(&run);
            CHECK(run.screen.kind == GC_SCREEN_REWARD, "screen %d, want the reward screen", run.screen.kind);
            CHECK(run.screen.n_options == 0, "%d reward options, want none (heal-only shop)", run.screen.n_options);
            if (option < 2) {
                gc_rphase p;
                CHECK(gc_rq_next(q, &p) && p.type == RP_ME_POST, "the post-option phase is not next after the shop");
                rng_at_post = run.run.rng;
                if (p.type == RP_ME_POST) gc_mystery_post(&run, &p);
            }
        }
        collect();
        if (option < 2) CHECK(same_rng(&run.run.rng, &rng_at_post), "the run RNG moved during the post-option phase");
        CHECK(same_rng(&after_post, &after_option), "upstream's run RNG moved during the post-option phase");
        if (has_post) {
            for (int m = 0; m < n_mons; m++) {
                mon_state cs;
                c_state(&run.party[m], &cs);
                states_checked++;
                if (!same_state(&cs, &after_post_state[m])) { CHECK(0, "Pokemon %d after the post-option phase differs", m); if (shown <= 14) { print_state("C", &cs); print_state("upstream", &after_post_state[m]); } }
            }
        }
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) { CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]); msgs_checked++; }
        CHECK(sys.n_eggs == n_eggs, "%d eggs", sys.n_eggs);
        for (int k = 0; k < n_eggs && k < sys.n_eggs; k++) CHECK(sys.eggs[k].hatch_waves == up_eggs[k], "egg %d has %d waves left, upstream %d", k, sys.eggs[k].hatch_waves, up_eggs[k]);
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Shady Vitamin Dealer: %d cases run (options %d/%d/%d), %d offered nothing the case could run; %d starting parties, %d party-screen filters, %d Pokemon states, %d messages compared. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], disabled, setup_checked, filters_checked, states_checked, msgs_checked, mismatches);
    return mismatches ? 1 : 0;
}
