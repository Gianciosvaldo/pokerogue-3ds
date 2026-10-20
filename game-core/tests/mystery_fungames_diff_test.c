/* Fun and Games! against upstream at 8555c08: replays the "-fungames" trace of tests/mystery-fungames.producer.test.ts, which plays
 * the REAL encounter through the game manager's real phases (the party screen, MysteryEncounterOptionSelectedPhase, summonPlayerPokemon,
 * the Wobbuffet made by addEnemyPokemon, TurnInitPhase's handleNextTurn, the AI's move choice, the three turns of the player's own
 * FIGHT commands, VictoryPhase and the lose handler, the shop the encounter ends on) through game-core's RP_MYSTERY screen,
 * gc_fg_start, battle-core's turns and gc_fg_turn_init / gc_fg_lose.
 *
 * Per case, compared with what upstream did: which options are enabled; the party's stats (the setup is rebuilt from species,
 * level, IVs, nature, ability and moves); the Wobbuffet's level, HP, stats, IVs, nature, ability, shininess and its moveset in
 * order, the run RNG after it was made and the money paid; per turn the Wobbuffet's HP and the lead's HP and the run RNG; the
 * encounter's messages in order; the money; the party after (order, level, experience, friendship, HP, moves); Wobbuffet's dex
 * entry; the shop it ends on (its options, tiers and upgrade counts) with the run RNG after it was rolled.
 * Usage: mystery_fungames_diff_test <trace>; exit 0 only with 0 mismatches. */
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
static void semicolon(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, ";")) { fprintf(stderr, "wrong record: wanted ;, got %s\n", s); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

#define MAX_MSG 24
static char msgs[MAX_MSG][64];
static int n_msgs;
static const char *const watched[] = {
    "mysteryEncounterMessages:paidMoney",
    "mysteryEncounters/funAndGames:ko", "mysteryEncounters/funAndGames:endGame", "mysteryEncounters/funAndGames:bestResult",
    "mysteryEncounters/funAndGames:greatResult", "mysteryEncounters/funAndGames:goodResult", "mysteryEncounters/funAndGames:badResult",
    "mysteryEncounters/funAndGames:turnRemaining1", "mysteryEncounters/funAndGames:turnRemaining2", "mysteryEncounters/funAndGames:turnRemaining3",
    "mysteryEncounters/funAndGames:chargingContinue",
};
static void collect(void)
{
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
        for (size_t i = 0; i < sizeof watched / sizeof watched[0]; i++)
            if (!strcmp(e.msg.key, watched[i]) && n_msgs < MAX_MSG) { snprintf(msgs[n_msgs++], sizeof msgs[0], "%s", e.msg.key); break; }
    }
}

typedef struct { int species, form, level, ivs[6], nature, ability; double exp; int friendship, stats[6], hp, n_moves, moves[4]; } up_mon;
static up_mon read_mon(void)
{
    up_mon u;
    memset(&u, 0, sizeof u);
    u.species = (int)integer(); u.form = (int)integer(); u.level = (int)integer();
    for (int k = 0; k < 6; k++) u.ivs[k] = (int)integer();
    u.nature = (int)integer(); u.ability = (int)integer(); u.exp = real(); u.friendship = (int)integer();
    for (int k = 0; k < 6; k++) u.stats[k] = (int)integer();
    u.hp = (int)integer(); u.n_moves = (int)integer();
    for (int k = 0; k < u.n_moves && k < 4; k++) u.moves[k] = (int)integer();
    return u;
}

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < 14) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

static const gc_mon *lead(void) { return &run.party[run.party_order[0]]; }
static pr_pokemon *wobbuffet(void) { return run.bt.field[2]; }

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_fungames_diff_test <trace>\n", stderr); return 2; }
    tag('V');
    integer();
    integer();
    int cases = 0, mismatches = 0, by_option[2] = {0}, disabled = 0, kos = 0, prizes[4] = {0}, turns_checked = 0, msgs_checked = 0, moves_checked = 0, shops_checked = 0, levelups = 0;
    char c;
    while (fscanf(f, " %c", &c) == 1) {
        if (c == 'Z') break;
        if (c != 'C') { fprintf(stderr, "wrong record %c\n", c); return 2; }
        char run_seed[64], wave_seed[64], battle_seed[32];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed); word(battle_seed, sizeof battle_seed);
        const int wave = (int)integer();
        const double money_start = real();
        const int option = (int)integer(), pick = (int)integer(), n_mons = (int)integer(), runs = (int)integer(), mode = (int)integer(), biome = (int)integer();
        (void)mode;
        case_option = option; case_wave = wave;
        if (n_mons < 1 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
        tag('U');
        up_mon setup[6];
        for (int m = 0; m < n_mons; m++) { if (m) semicolon(); setup[m] = read_mon(); }
        tag('A');
        const int meets[2] = {(int)integer(), (int)integer()};
        const double cost = real();
        tag('D');
        integer();
        const unsigned long long dex_seen_attr = (unsigned long long)integer();
        const int dex_seen_count = (int)integer();
        const unsigned long long dex_caught_attr = (unsigned long long)integer();
        const int dex_caught_count = (int)integer();

        pr_rng before = {0}, wob_rng = {0}, shop_rng = {0};
        double wob_money = 0, money_after = 0;
        up_mon wob;
        int wob_hp = 0, wob_shiny = 0, wob_moves[4] = {0}, wob_n_moves = 0;
        struct { int ov_max, ov, slot, wob_hp, lead_hp; pr_rng rng; } turns[8];
        int n_turns = 0, n_up_msgs = 0, shop_count = 0, n_shop_opts = 0, n_order = 0, order[6];
        char up_msgs[MAX_MSG][64];
        struct { char key[40]; int arg, tier, upgrade; } want[8];
        up_mon after[6];
        pr_rng after_shop = {0};
        unsigned long long x_seen_attr = 0, x_caught_attr = 0;
        int x_seen_count = 0, x_caught_count = 0;
        memset(&wob, 0, sizeof wob);
        if (runs) {
            tag('R'); before = rng_vals();
            if (option == 0) {
                tag('W');
                wob.level = (int)integer(); wob_hp = (int)integer();
                for (int k = 0; k < 6; k++) wob.stats[k] = (int)integer();
                for (int k = 0; k < 6; k++) wob.ivs[k] = (int)integer();
                wob.nature = (int)integer(); wob.ability = (int)integer(); wob_shiny = (int)integer();
                wob_n_moves = (int)integer();
                for (int k = 0; k < wob_n_moves && k < 4; k++) wob_moves[k] = (int)integer();
                wob_money = real(); wob_rng = rng_vals();
                tag('L');
                n_turns = (int)integer();
                if (n_turns < 1 || n_turns > 8) { fputs("turn count\n", stderr); return 2; }
                for (int k = 0; k < n_turns; k++) {
                    tag('T');
                    turns[k].ov_max = (int)integer(); turns[k].ov = (int)integer(); turns[k].slot = (int)integer(); turns[k].wob_hp = (int)integer(); turns[k].lead_hp = (int)integer();
                    turns[k].rng = rng_vals();
                }
            }
            tag('Q');
            money_after = real(); shop_rng = rng_vals();
            n_up_msgs = (int)integer();
            for (int k = 0; k < n_up_msgs; k++) word(up_msgs[k], sizeof up_msgs[k]);
            tag('S');
            shop_count = (int)integer(); n_shop_opts = (int)integer();
            if (n_shop_opts > 8) { fputs("shop size\n", stderr); return 2; }
            for (int k = 0; k < n_shop_opts; k++) {
                word(want[k].key, sizeof want[k].key);
                want[k].arg = (int)integer(); want[k].tier = (int)integer(); want[k].upgrade = (int)integer();
            }
            tag('G'); after_shop = rng_vals();
            tag('P');
            for (int k = 0; k < n_mons; k++) order[n_order++] = (int)integer();
            for (int k = 0; k < n_mons; k++) {
                semicolon();
                memset(&after[k], 0, sizeof after[k]);
                after[k].species = (int)integer(); after[k].level = (int)integer(); after[k].exp = real(); after[k].friendship = (int)integer();
                after[k].hp = (int)integer(); after[k].n_moves = (int)integer();
                for (int j = 0; j < after[k].n_moves && j < 4; j++) after[k].moves[j] = (int)integer();
            }
            tag('X');
            integer();
            x_seen_attr = (unsigned long long)integer(); x_seen_count = (int)integer(); x_caught_attr = (unsigned long long)integer(); x_caught_count = (int)integer();
        }
        tag('E');

        /* ---- the run as the encounter finds it ---- */
        memset(&run, 0, sizeof run);
        gc_run_new(&run, GAME_MODES_CLASSIC, run_seed, 42);
        gc_starter team[6];
        memset(team, 0, sizeof team);
        for (int m = 0; m < n_mons; m++) {
            team[m].species = (uint16_t)setup[m].species;
            team[m].form = (uint8_t)setup[m].form;
            team[m].ability_index = (uint8_t)setup[m].ability;
            team[m].nature = (uint8_t)setup[m].nature;
            team[m].tera_type = -1;
            for (int k = 0; k < 6; k++) team[m].ivs[k] = (uint8_t)setup[m].ivs[k];
            for (int k = 0; k < setup[m].n_moves && k < 4; k++) team[m].moves[k] = (uint16_t)setup[m].moves[k];
        }
        gc_system_init_defaults(&sys);
        run.sys = &sys;
        run.n_party = n_mons;
        run.n_items = 0;
        memset(run.items, 0, sizeof run.items);
        run.wave = wave;
        run.biome = biome;
        gc_items_to_battle(&run);
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[m];
            gc_mon_new_player(&run, mon, &team[m], setup[m].level);
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.on_field = false;
            mon->pause_evolutions = true;
            mon->b.friendship = (uint8_t)setup[m].friendship;
            mon->exp = setup[m].exp;
            run.party_order[m] = (int8_t)m;
            pr_calculate_stats(&run.bt, &mon->b);
            mon->b.hp = setup[m].hp;
        }
        run.money = money_start;
        run.battle_type = BATTLE_TYPE_WILD;
        gc_dex_entry *wd = &sys.dex[SPECIES_ID_WOBBUFFET];
        wd->seen_attr = dex_seen_attr; wd->seen_count = (uint32_t)dex_seen_count; wd->caught_attr = dex_caught_attr; wd->caught_count = (uint32_t)dex_caught_count;
        bad = 0;
        for (int m = 0; m < n_mons; m++) {
            char name[48];
            for (int k = 0; k < 6; k++) { snprintf(name, sizeof name, "party %d stat %d", m, k); EQ(name, run.party[m].b.stats[k], setup[m].stats[k]); }
        }
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        memset(run.battle_seed, 0, sizeof run.battle_seed);
        snprintf(run.battle_seed, sizeof run.battle_seed, "%.16s", battle_seed);
        run.run.rng = runs ? before : run.run.rng;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_FUN_AND_GAMES;
        gc_rq_unshift(&run, gc_rp(RP_MYSTERY, GC_ME_FUN_AND_GAMES, GC_ME_GREAT, 0));
        const char *why;
        gc_run_until_screen(&run);
        CHECK(run.screen.kind == GC_SCREEN_MYSTERY && run.screen.n_options == 2, "screen %d with %d options", run.screen.kind, run.screen.n_options);
        CHECK(run.screen.option_ok[0] == (meets[0] != 0) && run.screen.option_ok[1] == (meets[1] != 0), "options enabled %d/%d, upstream %d/%d",
              run.screen.option_ok[0], run.screen.option_ok[1], meets[0], meets[1]);
        EQ("cost", pr_run_get_wave_money_amount(wave, 1.5), cost);
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
        /* the party screen's pick rides on the choice (b) */
        gc_choice ch = {GC_CH_MYSTERY, (int16_t)option, (int16_t)(option == 0 ? pick : 0), 0};
        if (!gc_run_choose(&run, &ch, &why)) { CHECK(false, "option refused: %s", why); if (bad) mismatches++; cases++; continue; }
        collect();
        bool ok = true;
        if (ok && option == 0) {
            CHECK(run.screen.kind == GC_SCREEN_COMMAND, "screen %d after the pick, not the FIGHT menu", run.screen.kind);
            CHECK(run.screen.fight_only && !run.screen.can_run && !run.screen.can_ball && !run.screen.can_switch, "menu is not FIGHT-only (%d %d %d %d)",
                  run.screen.fight_only, run.screen.can_run, run.screen.can_ball, run.screen.can_switch);
            const pr_pokemon *w = wobbuffet();
            CHECK(w != NULL && run.n_enemy == 1, "no Wobbuffet on the field");
            if (w && run.screen.kind == GC_SCREEN_COMMAND) {
                EQ("wobbuffet level", w->level, wob.level);
                EQ("wobbuffet hp", w->hp, wob_hp);
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "wobbuffet stat %d", k); EQ(n, w->stats[k], wob.stats[k]); }
                for (int k = 0; k < 6; k++) { char n[32]; snprintf(n, sizeof n, "wobbuffet iv %d", k); EQ(n, w->ivs[k], wob.ivs[k]); }
                EQ("wobbuffet nature", w->nature, wob.nature);
                EQ("wobbuffet ability index", run.enemy[0].ability_index, wob.ability);
                EQ("wobbuffet shiny", w->shiny, wob_shiny);
                EQ("wobbuffet move count", w->n_moves, wob_n_moves);
                for (int k = 0; k < wob_n_moves && k < 4; k++) { char n[32]; snprintf(n, sizeof n, "wobbuffet move %d", k); EQ(n, w->moves[k], wob_moves[k]); moves_checked++; }
                EQ("money paid", run.money, wob_money);
                CHECK(same_rng(&run.run.rng, &wob_rng), "the run RNG after the Wobbuffet differs (c %.0f vs %.0f)", run.run.rng.c, wob_rng.c);
            }
            /* ---- the turns ---- */
            for (int t = 0; t < n_turns && run.screen.kind == GC_SCREEN_COMMAND; t++) {
                if (turns[t].ov_max > 0 && wobbuffet()) wobbuffet()->stats[0] = turns[t].ov_max;
                if (turns[t].ov >= 0 && wobbuffet()) wobbuffet()->hp = turns[t].ov;
                gc_choice fc = {GC_CH_FIGHT, (int16_t)turns[t].slot, -1, 0};
                if (!gc_run_choose(&run, &fc, &why)) { CHECK(false, "turn %d command refused: %s", t, why); break; }
                collect();
                turns_checked++;
                const bool last = t == n_turns - 1;
                const pr_pokemon *w2 = wobbuffet();
                /* the Wobbuffet's HP: while it is on the field; it is gone (removed) once the encounter ended */
                if (!last) CHECK(run.screen.kind == GC_SCREEN_COMMAND, "turn %d ends on screen %d, not the next FIGHT menu", t, run.screen.kind);
                if (w2 && run.screen.kind == GC_SCREEN_COMMAND) EQ("wobbuffet hp after the turn", w2->hp, turns[t].wob_hp);
                else if (!last) EQ("wobbuffet hp after the turn", 0, turns[t].wob_hp);
                EQ("lead hp after the turn", lead()->b.hp, turns[t].lead_hp);
                if (!last) CHECK(same_rng(&run.run.rng, &turns[t].rng), "turn %d: the run RNG moved", t);
            }
        }
        if (ok) {
            CHECK(run.screen.kind == GC_SCREEN_REWARD, "ends on screen %d, not the shop", run.screen.kind);
            if (run.screen.kind == GC_SCREEN_REWARD) {
                CHECK(run.screen.n_options == n_shop_opts, "%d shop options, upstream %d", run.screen.n_options, n_shop_opts);
                for (int k = 0; k < n_shop_opts && k < run.screen.n_options; k++) {
                    const gc_offer *o = &run.screen.options[k];
                    CHECK(!strcmp(o->key, want[k].key) && o->tier == want[k].tier && o->upgrade_count == want[k].upgrade && o->arg == want[k].arg,
                          "shop option %d is %s/%d tier %d up %d, upstream %s/%d tier %d up %d", k, o->key, o->arg, o->tier, o->upgrade_count,
                          want[k].key, want[k].arg, want[k].tier, want[k].upgrade);
                    shops_checked++;
                }
                (void)shop_count;
                CHECK(same_rng(&run.run.rng, &after_shop), "the run RNG after the shop differs (c %.0f vs %.0f)", run.run.rng.c, after_shop.c);
            }
            EQ("money", run.money, money_after);
            CHECK(n_msgs == n_up_msgs, "%d messages, upstream %d", n_msgs, n_up_msgs);
            for (int k = 0; k < n_msgs && k < n_up_msgs; k++) {
                fields_checked++;
                CHECK(!strcmp(msgs[k], up_msgs[k]), "message %d is %s, upstream %s", k, msgs[k], up_msgs[k]);
                msgs_checked++;
            }
            EQ("party size", run.n_party, n_order);
            for (int k = 0; k < run.n_party && k < n_order; k++) {
                const gc_mon *m = &run.party[run.party_order[k]];
                const int idx = m->b.id >= 0x4000 && m->b.id < 0x4000 + 6 ? (int)m->b.id - 0x4000 : -1;
                EQ("party slot", idx, order[k]);
                char n[48];
                snprintf(n, sizeof n, "party %d species", k); EQ(n, m->b.species, after[k].species);
                snprintf(n, sizeof n, "party %d level", k); EQ(n, m->b.level, after[k].level);
                snprintf(n, sizeof n, "party %d exp", k); fields_checked++; CHECK(m->exp == after[k].exp, "%s is %.17g, upstream %.17g", n, m->exp, after[k].exp);
                snprintf(n, sizeof n, "party %d friendship", k); EQ(n, m->b.friendship, after[k].friendship);
                snprintf(n, sizeof n, "party %d hp", k); EQ(n, m->b.hp, after[k].hp);
                snprintf(n, sizeof n, "party %d move count", k); EQ(n, m->b.n_moves, after[k].n_moves);
                for (int j = 0; j < after[k].n_moves && j < 4; j++) { snprintf(n, sizeof n, "party %d move %d", k, j); EQ(n, m->b.moves[j], after[k].moves[j]); moves_checked++; }
                if (m->b.level != setup[idx >= 0 ? idx : 0].level) levelups++;
            }
            EQ("dex seen attr", wd->seen_attr, x_seen_attr);
            EQ("dex seen count", wd->seen_count, x_seen_count);
            EQ("dex caught attr", wd->caught_attr, x_caught_attr);
            EQ("dex caught count", wd->caught_count, x_caught_count);
            for (int k = 0; k < n_up_msgs; k++) {
                if (!strcmp(up_msgs[k], "mysteryEncounters/funAndGames:ko")) { kos++; break; }
            }
            for (int k = 0; k < n_up_msgs; k++) {
                if (strstr(up_msgs[k], "bestResult")) prizes[0]++;
                else if (strstr(up_msgs[k], "greatResult")) prizes[1]++;
                else if (strstr(up_msgs[k], "goodResult")) prizes[2]++;
                else if (strstr(up_msgs[k], "badResult")) prizes[3]++;
            }
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
    }
    printf("Mystery encounter Fun and Games: %d cases run (play %d, leave %d), %d not runnable (money under the price), %d knocked-out Wobbuffets, prizes best/great/good/bad %d/%d/%d/%d, "
           "%d turns, %d moves, %d messages, %d shop options, %d level-ups, %d fields compared. TOTAL mismatches %d\n",
           cases - disabled, by_option[0], by_option[1], disabled, kos, prizes[0], prizes[1], prizes[2], prizes[3], turns_checked, moves_checked, msgs_checked, shops_checked, levelups,
           fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
