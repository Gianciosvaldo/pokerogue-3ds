/* The Pokemon Salesman against upstream at 8555c08: replays the "-salesman" trace of tests/mystery-salesman.producer.test.ts
 * (the real encounter's onInit run again in executeWithSeedOffset(.., wave), the option's meetsRequirements(), and the
 * chosen option's real onOptionPhase in executeWithSeedOffset(.., * 500) with catchPokemon really run: the dex, the party,
 * the messages, the party-full prompt answered through the real UI handlers, the VictoryPhase it unshifted really started)
 * through game-core's RP_MYSTERY screen, gc_mystery_execute, RP_CATCH and the heal-only shop it ends on.
 *
 * Per case, compared with what upstream did: the Pokemon on sale (its id, species, form, level, ability index, gender,
 * shininess, variant, nature, IVs, tera type, luck, friendship, experience, stats and HP; not its moveset, which game-core
 * has no port of for a player's Pokemon: the stand-in's) and its price; which options are enabled; the run RNG (nothing is
 * drawn); money; the messages in order; the party after (who is left, in what order, and the Pokemon bought with every
 * field above plus the ball and where it was met); the dex entries and candy of the species' chain; and that the experience
 * phase of the VictoryPhase is empty and the encounter leaves with the heal-only shop.
 * Usage: mystery_salesman_diff_test <trace>; exit 0 only with 0 mismatches. */
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
    char c = '?';
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

#define MAX_MSG 12
static char msgs[MAX_MSG][32];
static int n_msgs;
static void collect(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key || n_msgs >= MAX_MSG) continue;
        const char *k = strrchr(e.msg.key, ':');
        k = k ? k + 1 : e.msg.key;
        if (!strcmp(k, "swapOrRelease")) continue;   /* the app's marker for the party-full prompt, not upstream's */
        if (!strncmp(e.msg.key, "partyUiHandler:", 15)) k = "farewell";
        snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", k);
    }
}

typedef struct {
    long long id;
    int species, form, level, hp, stats[6], ball, met_wave, met_level, met_biome, met_species, ivs[6], nature, ability, gender, shiny, variant, friendship, luck;
    double exp;
    int tera, passive, pokerus;
} up_mon;
static up_mon read_mon(void)
{
    up_mon u;
    u.id = integer(); u.species = (int)integer(); u.form = (int)integer(); u.level = (int)integer(); u.hp = (int)integer();
    for (int k = 0; k < 6; k++) u.stats[k] = (int)integer();
    u.ball = (int)integer(); u.met_wave = (int)integer(); u.met_level = (int)integer(); u.met_biome = (int)integer(); u.met_species = (int)integer();
    for (int k = 0; k < 6; k++) u.ivs[k] = (int)integer();
    u.nature = (int)integer(); u.ability = (int)integer(); u.gender = (int)integer(); u.shiny = (int)integer(); u.variant = (int)integer();
    u.friendship = (int)integer(); u.luck = (int)integer(); u.exp = real(); u.tera = (int)integer(); u.passive = (int)integer(); u.pokerus = (int)integer();
    return u;
}

typedef struct { int id; unsigned long long caught, nature, count, seen_attr, seen_count; int ivs[6], candy, abil; } up_dex;
static up_dex read_dex(void)
{
    up_dex d;
    d.id = (int)integer(); d.caught = (unsigned long long)integer(); d.nature = (unsigned long long)integer(); d.count = (unsigned long long)integer();
    for (int k = 0; k < 6; k++) d.ivs[k] = (int)integer();
    d.candy = (int)integer(); d.abil = (int)integer(); d.seen_attr = (unsigned long long)integer(); d.seen_count = (unsigned long long)integer();
    return d;
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)

static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

/* every number of a Pokemon the trace has (never the moveset) */
static void check_mon(const gc_mon *m, const up_mon *u, const char *what)
{
    const pr_pokemon *p = &m->b;
    char name[64];
#define F(label, c_val, up_val) do { snprintf(name, sizeof name, "%s %s", what, label); EQ(name, c_val, up_val); } while (0)
    F("id", p->id, u->id); F("species", p->species, u->species); F("form", p->form, u->form); F("level", p->level, u->level); F("hp", p->hp, u->hp);
    for (int k = 0; k < 6; k++) { char l[16]; snprintf(l, sizeof l, "stat %d", k); F(l, p->stats[k], u->stats[k]); }
    F("ball", m->pokeball, u->ball); F("met wave", m->met_wave, u->met_wave); F("met level", m->met_level, u->met_level);
    F("met biome", m->met_biome, u->met_biome); F("met species", m->met_species, u->met_species);
    for (int k = 0; k < 6; k++) { char l[16]; snprintf(l, sizeof l, "iv %d", k); F(l, p->ivs[k], u->ivs[k]); }
    F("nature", p->nature, u->nature); F("ability index", m->ability_index, u->ability); F("gender", p->gender, u->gender);
    F("shiny", p->shiny, u->shiny); F("variant", m->variant, u->variant); F("friendship", p->friendship, u->friendship);
    F("luck", m->luck, u->luck); F("tera type", p->tera_type, u->tera); F("passive", p->passive_flag, u->passive); F("pokerus", m->pokerus, u->pokerus);
    snprintf(name, sizeof name, "%s exp", what);
    fields_checked++;
    CHECK(m->exp == u->exp, "%s is %.17g, upstream %.17g", name, m->exp, u->exp);
#undef F
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_salesman_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[2] = {0}, disabled = 0, bought = 0, magikarps = 0, shinies = 0, released = 0, declined = 0, with_room = 0, msgs_checked = 0,
        dex_checked = 0, offers_checked = 0, hidden = 0, starters_unlocked = 0, full = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c == 'P') {
            n_mons = (int)integer();
            if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            continue;
        }
        if (c != 'C' || !n_mons) { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer();
        const double money_start = real();
        const int trainer_id = (int)integer(), secret_id = (int)integer(), charms = (int)integer(), biome = (int)integer(), release = (int)integer(), n_party = (int)integer(), runs = (int)integer();
        case_option = option; case_wave = wave;
        if (n_party != n_mons) { fputs("party size changed\n", stderr); return 2; }
        struct { int species, form, level, alive; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) { setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].alive = (int)integer(); }
        tag('A');
        const int meets[2] = {(int)integer(), (int)integer()};
        const double price = real(), max_exp_level = real();
        const up_mon offer = read_mon();
        tag('D');
        const int n_chain = (int)integer();
        up_dex dex_before[8], dex_after[8];
        if (n_chain < 1 || n_chain > 8) { fputs("chain size\n", stderr); return 2; }
        for (int k = 0; k < n_chain; k++) dex_before[k] = read_dex();

        pr_rng before = {0}, after_option = {0}, after_victory = {0};
        double money_after = 0;
        int leave_arg = 0, n_queued = 0, n_up_msgs = 0, n_exp = 0, n_order = 0, order[6], has_new = 0;
        char queued[8][32], up_msgs[MAX_MSG][32], victory_id[16] = "-1";
        up_mon bought_mon;
        if (runs) {
            tag('R'); before = rng_vals();
            tag('Q'); after_option = rng_vals(); after_victory = rng_vals(); money_after = real(); leave_arg = (int)integer();
            tag('N');
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('X');
            n_queued = (int)integer();
            for (int k = 0; k < n_queued; k++) word(queued[k], sizeof queued[k]);
            word(victory_id, sizeof victory_id);
            tag('F');
            n_exp = (int)integer();
            for (int k = 0; k < n_exp * 3; k++) real();
            tag('W');
            n_order = (int)integer();
            for (int k = 0; k < n_order; k++) order[k] = (int)integer();
            char t = '?';
            if (fscanf(f, " %c", &t) != 1) { fputs("malformed input\n", stderr); return 2; }
            if (t == 'M') { has_new = 1; bought_mon = read_mon(); if (fscanf(f, " %c", &t) != 1) { fputs("malformed input\n", stderr); return 2; } }
            if (t != 'V') { fprintf(stderr, "wrong record: wanted V, got %c\n", t); return 2; }
            for (int k = 0; k < n_chain; k++) dex_after[k] = read_dex();
        }
        tag('E');
        (void)after_option;

        /* ---- the run as the case left it ---- */
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
        run.sys = &sys;
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            run.party[m] = tmpl;
            gc_mon *mon = &run.party[m];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)m;
        }
        run.n_party = n_mons;
        if (charms) {
            const int idx = gc_reward_item_index("SHINY_CHARM");
            gc_item *it = &run.items[run.n_items++];
            it->key = pr_item_keys[idx];
            it->arg = -1;
            it->pokemon_id = 0;
            it->stack = charms;
        }
        gc_items_to_battle(&run);
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[m];
            pr_calculate_stats(&run.bt, &mon->b);
            mon->b.hp = setup[m].alive ? mon->b.stats[0] : 0;
        }
        for (int k = 0; k < n_chain; k++) {
            const int id = dex_before[k].id;
            gc_dex_entry *d = &sys.dex[id];
            d->caught_attr = dex_before[k].caught; d->nature_attr = (uint32_t)dex_before[k].nature; d->caught_count = (uint32_t)dex_before[k].count;
            d->seen_attr = dex_before[k].seen_attr; d->seen_count = (uint32_t)dex_before[k].seen_count;
            for (int j = 0; j < 6; j++) d->ivs[j] = (uint8_t)dex_before[k].ivs[j];
            if (dex_before[k].candy >= 0) sys.starters[id].candy_count = dex_before[k].candy;
            if (dex_before[k].abil >= 0) sys.starters[id].ability_attr = (uint32_t)dex_before[k].abil;
        }
        run.wave = wave;
        run.biome = biome;
        run.money = money_start;
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.battle_type = BATTLE_TYPE_WILD;
        bad = 0;
        EQ("level cap", pr_run_get_max_exp_level(&run.run, wave, false), max_exp_level);
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        if (runs) run.run.rng = before;
        const pr_rng rng_start = run.run.rng;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_THE_POKEMON_SALESMAN;
        gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_THE_POKEMON_SALESMAN, GC_ME_ULTRA, 0));
        gc_run_until_screen(&run);
        CHECK(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.n_options == 2, "screen %d with %d options", run.screen.kind, run.screen.n_options);
        CHECK(run.screen.option_ok[0] == (meets[0] != 0) && run.screen.option_ok[1] == (meets[1] != 0), "options enabled %d/%d, upstream %d/%d",
              run.screen.option_ok[0], run.screen.option_ok[1], meets[0], meets[1]);

        /* the Pokemon on sale and its price */
        {
            gc_mon on_sale;
            double c_price = 0;
            const bool made = gc_salesman_offer(&run, &on_sale, &c_price);
            CHECK(made, "no offer made");
            if (made) {
                EQ("price", c_price, price);
                check_mon(&on_sale, &offer, "offer");
                CHECK(on_sale.b.is_player == false, "the offer is not an enemy-shaped Pokemon");
                offers_checked++;
                if (on_sale.b.species == SPECIES_ID_MAGIKARP) magikarps++;
                if (on_sale.b.shiny) shinies++;
                if (on_sale.ability_index == 2) hidden++;
            }
            CHECK(same_rng(&run.run.rng, &rng_start), "working out the offer moved the run RNG");
        }
        if (!runs) {
            CHECK(!run.screen.option_ok[option], "option %d is enabled in C but upstream could not run it", option);
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        /* ---- the option ---- */
        n_msgs = 0;
        collect();
        n_msgs = 0;
        gc_choice ch = {GC_CH_MYSTERY, (int16_t)option, 0, 0};
        if (!gc_run_choose(&run, &ch, &why)) { CHECK(false, "option refused: %s", why); if (bad) mismatches++; cases++; continue; }
        collect();
        const bool prompted = run.screen.kind == GC_SCREEN_SWITCH;
        if (prompted) CHECK(same_rng(&run.run.rng, &before), "the run RNG moved before the party prompt was answered");
        CHECK(prompted == (option == 0 && release != -2), "party prompt %d, upstream expects %d", prompted, option == 0 && release != -2);
        for (int step = 0; prompted && step < 3 && run.screen.kind == GC_SCREEN_SWITCH; step++) {
            gc_choice a = {GC_CH_PICK, (int16_t)(release >= 0 ? release : -1), 0, 0};
            if (!gc_run_choose(&run, &a, &why)) { CHECK(false, "party answer refused: %s", why); break; }
            collect();
        }
        CHECK(run.screen.kind == GC_SCREEN_REWARD && gc_int(&run)->rq.current.type == RP_SELECT_MODIFIER && gc_int(&run)->rq.current.a == 1,
              "ends on screen %d, not the heal-only shop", run.screen.kind);
        CHECK(leave_arg == 1, "upstream left with addHealPhase %d", leave_arg);
        CHECK(same_rng(&before, &after_option) && same_rng(&before, &after_victory), "upstream's own RNG moved (%g)", before.s0);
        const pr_rng rng_end = run.run.rng;
        EQ("money", run.money, money_after);

        /* the phases upstream queued: the VictoryPhase of the Pokemon (id = the offered one), nothing else */
        const int want_queued = option == 0 ? 1 : 0;
        CHECK(n_queued == want_queued && (!want_queued || !strcmp(queued[0], "VictoryPhase")), "upstream queued %d phases", n_queued);
        CHECK(n_exp == 0, "upstream's VictoryPhase queued %d experience phases", n_exp);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int exp_queued = 0;
            while (gc_rq_next(&copy, &p)) if (p.type == RP_EXP) exp_queued++;
            CHECK(exp_queued == 0, "%d experience phases queued in C", exp_queued);
        }

        /* the messages, in order */
        CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
        for (int k = 0; k < n_msgs && k < n_up_msgs; k++) {
            fields_checked++;
            CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]);
            msgs_checked++;
        }
        for (int k = 0; k < n_up_msgs; k++) if (!strcmp(up_msgs[k], "addedAsAStarter")) { starters_unlocked++; break; }

        /* who is left, in party order, and the Pokemon bought */
        EQ("party size", run.n_party, n_order);
        int new_slot = -1;
        for (int k = 0; k < run.n_party && k < n_order; k++) {
            const gc_mon *m = &run.party[run.party_order[k]];
            const int idx = m->b.id >= 0x4000 && m->b.id < 0x4000 + 6 ? (int)m->b.id - 0x4000 : -1;
            if (idx < 0) new_slot = k;
            EQ("party slot", idx, order[k]);
        }
        EQ("a Pokemon was bought", new_slot >= 0, has_new);
        if (has_new && new_slot >= 0) check_mon(&run.party[run.party_order[new_slot]], &bought_mon, "bought");

        /* the dex entries and candy of the chain */
        for (int k = 0; k < n_chain; k++) {
            const int id = dex_after[k].id;
            const gc_dex_entry *d = &sys.dex[id];
            EQ("dex caught attr", d->caught_attr, dex_after[k].caught);
            EQ("dex natures", d->nature_attr, dex_after[k].nature);
            EQ("dex caught count", d->caught_count, dex_after[k].count);
            EQ("dex seen attr", d->seen_attr, dex_after[k].seen_attr);
            EQ("dex seen count", d->seen_count, dex_after[k].seen_count);
            for (int j = 0; j < 6; j++) EQ("dex iv", d->ivs[j], dex_after[k].ivs[j]);
            if (dex_after[k].candy >= 0) {
                EQ("candy", sys.starters[id].candy_count, dex_after[k].candy);
                EQ("ability attr", sys.starters[id].ability_attr, dex_after[k].abil);
            }
            dex_checked++;
        }

        /* the run RNG: only the heal-only shop may have drawn, so it must equal a bare heal-only shop started from the same RNG */
        {
            gc_rq_reset(&gc_int(&run)->rq);
            run.run.rng = before;
            gc_rq_unshift(&run, gc_rp(RP_SELECT_MODIFIER, 1, 0, 0));
            gc_run_until_screen(&run);
            CHECK(same_rng(&rng_end, &run.run.rng), "the run RNG after the option is not the bare heal-only shop's");
        }

        if (bad) mismatches++;
        cases++; by_option[option]++;
        if (option == 0) {
            bought += has_new;
            if (release >= 0) released++;
            else if (release == -1) declined++;
            else with_room++;
            if (release != -2) full++;
        }
    }
    printf("Mystery encounter The Pokemon Salesman: %d cases run (buy %d, leave %d), %d not runnable (money under the 4x requirement), %d offers compared "
           "(%d Magikarp, %d shiny, %d with a hidden ability), %d bought (%d with room, %d released one, %d answered No to a full party), %d dex entries, "
           "%d messages, %d unlocked a starter, %d fields compared. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], disabled, offers_checked, magikarps, shinies, hidden, bought, with_room, released, declined, dex_checked,
           msgs_checked, starters_unlocked, fields_checked, mismatches);
    (void)full;
    return mismatches ? 1 : 0;
}
