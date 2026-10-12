/* Lost at Sea against upstream at 8555c08: replays the "-lost" trace of tests/mystery-actions.producer.test.ts (the real
 * encounter's CanLearnMoveRequirement queries, the chosen option's onOptionPhase in executeWithSeedOffset(.., * 500), the
 * rewards phase's doEncounterRewardsAndContinue with the PartyExpPhase really started, EggLapsePhase.start()) through
 * game-core's gc_mystery_setup_screen / gc_mystery_execute.
 *
 * Per case, compared with what upstream did:
 *  - the party at the start (max HP and the six stats C calculates from the same level, IVs, nature and fusion)
 *  - what the options offer: each option's meetsRequirements() as the screen's option_ok, the Pokemon each requirement
 *    lets through (Surf, Fly) as gc_can_learn_move, and the primary Pokemon the option picks
 *  - the run RNG and money after the option (neither moves), that upstream left with no heal shop
 *  - the phases queued: a PartyExpPhase for Surf / Fly only (the wander option gives nothing), and the experience phases
 *    it unshifts: which party members, in which order, with exactly which amount (the level cap, Exp. Share, Exp.
 *    Balance, Oval Charm, Lucky / Golden Egg, Pokerus and the wave multiplier all go into it)
 *  - every Pokemon's HP afterwards (the wander option's 25% damage, never fainting a Pokemon) and the eggs' wave counters
 * Usage: mystery_lost_diff_test <trace>; exit 0 only with 0 mismatches. */
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

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_lost_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, shown = 0, by_option[3] = {0}, disabled = 0, setup_checked = 0, masks_checked = 0, exp_checked = 0,
        primaries_checked = 0, hp_checked = 0, exp_phases = 0, fainted_primary = 0;
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
        const int runs = (int)integer();
        struct { int species, form, fusion, fusion_form, level, nature, ivs[6], pokerus, ability, passive, hp, max, stats[6]; } setup[6];
        tag('U');
        for (int m = 0; m < n_mons; m++) {
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].fusion = (int)integer();
            setup[m].fusion_form = (int)integer(); setup[m].level = (int)integer(); setup[m].nature = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].ivs[k] = (int)integer();
            setup[m].pokerus = (int)integer(); setup[m].ability = (int)integer(); setup[m].passive = (int)integer(); setup[m].hp = (int)integer(); setup[m].max = (int)integer();
            for (int k = 0; k < 6; k++) setup[m].stats[k] = (int)integer();
        }
        tag('I');
        const int share = (int)integer(), balance = (int)integer(), oval = (int)integer(), n_egg_items = (int)integer();
        int egg_item[12][3];
        for (int k = 0; k < n_egg_items; k++) for (int j = 0; j < 3; j++) egg_item[k][j] = (int)integer();
        tag('G');
        const int n_eggs = (int)integer();
        int eggs[8];
        for (int k = 0; k < n_eggs; k++) eggs[k] = (int)integer();
        tag('A');
        int meets[3], masks[2], primaries[2];
        for (int k = 0; k < 3; k++) meets[k] = (int)integer();
        for (int k = 0; k < 2; k++) masks[k] = (int)integer();
        for (int k = 0; k < 2; k++) primaries[k] = (int)integer();
        const double max_exp_level = real();

        pr_rng before = {0}, after_option = {0};
        double money_after = 0;
        int leave_arg = 0, n_queued = 0, n_exp = 0, hp_after[6], up_eggs[8], n_up_eggs = 0;
        char queued[8][40];
        struct { int kind, index; double exp; } up_exp[6];
        if (runs) {
            tag('R'); before = rng_vals();
            tag('Q'); after_option = rng_vals(); money_after = real(); leave_arg = (int)integer();
            tag('W');
            for (int m = 0; m < n_mons; m++) hp_after[m] = (int)integer();
            tag('X');
            n_queued = (int)integer();
            for (int k = 0; k < n_queued; k++) word(queued[k], sizeof queued[k]);
            tag('F');
            n_exp = (int)integer();
            for (int k = 0; k < n_exp; k++) { up_exp[k].kind = (int)integer(); up_exp[k].index = (int)integer(); up_exp[k].exp = real(); }
            tag('H');
            {
                char w[24];
                for (int k = 0; k < n_eggs; k++) { word(w, sizeof w); up_eggs[n_up_eggs++] = atoi(w); }
                if (!n_eggs) word(w, sizeof w);   /* "-" */
            }
        }
        tag('E');

        /* the party's storage order is not its order: member m of upstream's party sits in slot st[m] (a rotation that
         * changes with the case), and run.party_order is the party order, as after a switch */
        int st[6], at[6];
        for (int m = 0; m < n_mons; m++) { st[m] = (m + i % n_mons) % n_mons; at[st[m]] = m; }
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
        const gc_mon tmpl = run.party[0];
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = (uint16_t)setup[m].fusion;
            mon->b.fusion_form = (uint8_t)setup[m].fusion_form;
            mon->b.level = (uint8_t)setup[m].level;
            mon->b.n_moves = 0;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
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
        for (int k = 0; k < n_egg_items; k++) gc_item_add(&run, egg_item[k][1] == 40 ? "LUCKY_EGG" : "GOLDEN_EGG", 0, run.party[st[egg_item[k][0]]].b.id, egg_item[k][2]);
        gc_items_to_battle(&run);
        int bad = 0;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", i, option, wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
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
        run.money = 12345;
        run.battle_type = BATTLE_TYPE_WILD;
        CHECK(pr_run_get_max_exp_level(&run.run, wave, false) == max_exp_level, "level cap %.0f, upstream %.0f", pr_run_get_max_exp_level(&run.run, wave, false), max_exp_level);
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_LOST_AT_SEA;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_LOST_AT_SEA;
        gc_mystery_setup_screen(&run);

        /* what the options offer */
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        for (int k = 0; k < 3; k++)
            CHECK(run.screen.option_ok[k] == (meets[k] != 0), "option %d is %s in C, meetsRequirements() upstream %d", k, run.screen.option_ok[k] ? "enabled" : "disabled", meets[k]);
        for (int k = 0; k < 2; k++) {
            for (int m = 0; m < n_mons; m++) {
                const bool learns = gc_can_learn_move(&run.party[st[m]], k == 0 ? MOVE_ID_SURF : MOVE_ID_FLY);
                CHECK(learns == (((masks[k] >> m) & 1) != 0), "Pokemon %d (species %d level %d fusion %d) %s %s in C, upstream %s", m, setup[m].species, setup[m].level, setup[m].fusion,
                      learns ? "can learn" : "cannot learn", k == 0 ? "Surf" : "Fly", ((masks[k] >> m) & 1) ? "can" : "cannot");
                masks_checked++;
            }
            const int slot = gc_mystery_primary_pokemon(&run, GC_ME_LOST_AT_SEA, k);
            const int primary = slot < 0 ? -1 : at[slot];
            CHECK(primary == primaries[k], "option %d: primary Pokemon %d in C, upstream %d", k, primary, primaries[k]);
            primaries_checked++;
        }
        if (!runs) {
            if (bad) mismatches++;
            cases++; disabled++;
            continue;
        }

        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option) && same_rng(&before, &after_option), "the run RNG moved (upstream's did not)");
        CHECK(run.money == money_after, "money %.0f, want %.0f", run.money, money_after);
        CHECK(leave_arg == 0, "upstream left with addHealPhase %d", leave_arg);
        for (int m = 0; m < n_mons; m++) {
            CHECK(run.party[st[m]].b.hp == hp_after[m], "Pokemon %d HP %d after the option, upstream %d", m, run.party[st[m]].b.hp, hp_after[m]);
            hp_checked++;
        }

        /* the queue: PartyExpPhase (Surf / Fly only), then the PostMysteryEncounterPhase; here the experience phases it
         * unshifted, in order, then the next wave */
        CHECK((option < 2) == (n_queued == 2 && !strcmp(queued[0], "PartyExpPhase")) && !strcmp(queued[n_queued - 1], "+PostMysteryEncounterPhase"),
              "upstream queued %d phases (first %s) for option %d", n_queued, queued[0], option);
        {
            gc_rqueue copy = gc_int(&run)->rq;
            gc_rphase p;
            int pos = 0, seen_exp = 0;
            while (gc_rq_next(&copy, &p)) {
                if (p.type == RP_EXP) {
                    if (seen_exp < n_exp) {
                        if (!(up_exp[seen_exp].index == at[p.a] && p.d == up_exp[seen_exp].exp && up_exp[seen_exp].kind == (p.b ? 0 : 1)))
                            CHECK(0, "experience phase %d is Pokemon %d for %.17g (%s), upstream Pokemon %d for %.17g (%s)", seen_exp, at[p.a], p.d, p.b ? "ExpPhase" : "ShowPartyExpBarPhase",
                                  up_exp[seen_exp].index, up_exp[seen_exp].exp, up_exp[seen_exp].kind ? "ShowPartyExpBarPhase" : "ExpPhase");
                    }
                    seen_exp++;
                    exp_phases++;
                } else {
                    CHECK(seen_exp == n_exp, "%d experience phases before phase %d, upstream %d", seen_exp, p.type, n_exp);
                    CHECK(p.type == RP_NEW_BATTLE || p.type == RP_SELECT_BIOME, "queued phase %d at position %d", p.type, pos);
                }
                pos++;
            }
            CHECK(seen_exp == n_exp, "%d experience phases queued, upstream %d", seen_exp, n_exp);
            exp_checked += n_exp ? 1 : 0;
            CHECK(option < 2 || n_exp == 0, "experience for the wander option");
        }
        CHECK(sys.n_eggs == n_eggs, "%d eggs", sys.n_eggs);
        for (int k = 0; k < n_eggs && k < sys.n_eggs; k++) CHECK(sys.eggs[k].hatch_waves == up_eggs[k], "egg %d has %d waves left, upstream %d", k, sys.eggs[k].hatch_waves, up_eggs[k]);
        if (option < 2 && primaries[option] >= 0 && setup[primaries[option]].hp <= 0) fainted_primary++;
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Lost at Sea: %d cases run (Surf %d, Fly %d, wander %d; %d with a fainted primary), %d offered nothing the case could run; %d starting parties, %d can-learn masks, %d primary Pokemon, %d cases with experience (%d phases), %d HP values compared. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], by_option[2], fainted_primary, disabled, setup_checked, masks_checked, primaries_checked, exp_checked, exp_phases, hp_checked, mismatches);
    return mismatches ? 1 : 0;
}
