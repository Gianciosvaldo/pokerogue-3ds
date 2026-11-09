/* The Winstrate Challenge against upstream at 8555c08: replays the "-winstrate" trace of tests/mystery-winstrate.producer.test.ts (the real encounter: onInit in
 * executeWithSeedOffset(.., waveIndex); option 1's onOptionPhase really run, then doContinueEncounter (endTrainerBattleAndShowDialogue) four times and a fifth;
 * the five trainers' fights made by initBattleWithEnemyConfig, the voucher and the Macho Brace reward recorded; or option 2, the refusal) through game-core.
 * The wins in between are not played: each step is what the won fight's RP_WINSTRATE phase does (gc_winstrate_next).
 * Usage: mystery_winstrate_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include "gc/gacha.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static gc_system sys;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
static double real(void) { double v; if (fscanf(f, "%lf", &v) != 1) { fprintf(stderr, "malformed input at byte %ld\n", ftell(f)); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(const char *want)
{
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, want)) { fprintf(stderr, "wrong record: wanted %s, got %s (byte %ld)\n", want, w, ftell(f)); exit(2); }
}
static void semicolon(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, ";")) { fprintf(stderr, "wrong record: wanted ;, got %s (byte %ld)\n", s, ftell(f)); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

typedef struct { char key[48]; int arg, holder, stack, transferable; } item_row;
typedef struct { int species, form, level, boss, segments, ivs[6], nature, ability, passive, shiny, variant, gender, friendship; long long id; int hp, stats[6], n_moves, moves[4]; } mon_row;
typedef struct { int ball, tera, ability, luck, slot, fusion; char name[40]; } x3_row;

typedef struct {
    pr_rng rng;
    int n_phases, n_returns;
    int n_said;
    char said[4][256];
    bool fight;
    int mode, dbl, n_levels, levels[6], trainer_type, variant, template_index;
    int n_effects, n_items;
    item_row items[40];
    int n_x;
    mon_row mon[6];
    x3_row x3[6];
} step_t;

static void read_mon(mon_row *b)
{
    b->species = (int)integer(); b->form = (int)integer(); b->level = (int)integer();
    b->boss = (int)integer(); b->segments = (int)integer();
    for (int k = 0; k < 6; k++) b->ivs[k] = (int)integer();
    b->nature = (int)integer(); b->ability = (int)integer(); b->passive = (int)integer(); b->shiny = (int)integer(); b->variant = (int)integer();
    b->gender = (int)integer(); b->friendship = (int)integer(); b->id = integer();
    b->hp = (int)integer();
    integer();   /* max HP: stats[0] */
    for (int k = 0; k < 6; k++) b->stats[k] = (int)integer();
    b->n_moves = (int)integer();
    for (int k = 0; k < b->n_moves; k++) { const int mv = (int)integer(); if (k < 4) b->moves[k] = mv; }
}

static void read_step(const char *label, step_t *s)
{
    memset(s, 0, sizeof *s);
    tag(label);
    s->rng = rng_vals();
    tag("Ph");
    s->n_phases = (int)integer();
    for (int k = 0; k < s->n_phases; k++) {
        char ph[160];
        word(ph, sizeof ph);
        if (!strncmp(ph, "u:ReturnPhase", 13)) s->n_returns++;
    }
    tag("Sd");
    s->n_said = (int)integer();
    for (int k = 0; k < s->n_said; k++) { char m[256]; word(m, sizeof m); if (k < 4) snprintf(s->said[k], sizeof s->said[k], "%s", m); }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "D")) { fprintf(stderr, "wrong record: wanted D, got %s\n", w); exit(2); }
    char first[16];
    word(first, sizeof first);
    if (!strcmp(first, "none")) return;
    s->fight = true;
    s->mode = atoi(first);
    s->dbl = (int)integer();
    s->n_levels = (int)integer();
    for (int k = 0; k < s->n_levels; k++) { const int l = (int)integer(); if (k < 6) s->levels[k] = l; }
    s->trainer_type = (int)integer(); s->variant = (int)integer(); s->template_index = (int)integer();
    char t1[48], t2[96], t3[48];
    word(t1, sizeof t1); word(t2, sizeof t2); word(t3, sizeof t3);
    tag("F");
    s->n_effects = (int)integer();
    tag("J");
    s->n_items = (int)integer();
    for (int k = 0; k < s->n_items; k++) {
        item_row *r = &s->items[k];
        r->holder = (int)integer();
        word(r->key, sizeof r->key);
        r->arg = (int)integer(); r->stack = (int)integer(); r->transferable = (int)integer();
    }
    tag("X");
    s->n_x = (int)integer();
    for (int k = 0; k < s->n_x && k < 6; k++) { if (k) semicolon(); read_mon(&s->mon[k]); }
    tag("X3");
    integer();
    for (int k = 0; k < s->n_x && k < 6; k++) {
        if (k) semicolon();
        x3_row *x = &s->x3[k];
        x->ball = (int)integer(); x->tera = (int)integer(); x->ability = (int)integer(); x->luck = (int)integer(); x->slot = (int)integer(); x->fusion = (int)integer();
        word(x->name, sizeof x->name);
    }
    tag("N");
    for (int k = 0; k < s->n_x; k++) { if (k) semicolon(); integer(); integer(); integer(); }
}

static int bad, shown, case_no, case_wave, step_no;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < (getenv("SHOW") ? atoi(getenv("SHOW")) : 14)) { shown++; fprintf(stderr, "case %d (wave %d, step %d): ", case_no, case_wave, step_no); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

static int enemies_checked, items_checked, moves_checked, forms, fights, females, hp_checked;

/* the fight game-core has made against upstream's: trainer, levels, every Pokemon and every item */
static void compare_fight(const step_t *s, int k)
{
    static const char *const tname[5] = {"Victor", "Victoria", "Vivi", "Vicky", "Vito"};
    static const int trainer_of[5] = {TRAINER_TYPE_VICTOR, TRAINER_TYPE_VICTORIA, TRAINER_TYPE_VIVI, TRAINER_TYPE_VICKY, TRAINER_TYPE_VITO};
    CHECK(s->fight, "upstream made no fight");
    if (!s->fight) return;
    fights++;
    CHECK(run.n_enemy == s->n_x && run.in_battle && run.me_battle, "no battle (%d enemies, upstream %d)", run.n_enemy, s->n_x);
    EQ("trainer battle", run.battle_type, BATTLE_TYPE_TRAINER);
    EQ("single battle", run.double_battle, s->dbl);
    EQ("trainer type", run.trainer_type, s->trainer_type);
    EQ("trainer type (the order)", run.trainer_type, trainer_of[k]);
    EQ("trainer variant", run.trainer_variant == TRAINER_VARIANT_FEMALE, s->variant == 1);
    females += s->variant == 1;
    EQ("party template", s->template_index, 0);
    EQ("enemy levels", s->n_levels, s->n_x);
    EQ("battle effects", gc_int(&run)->n_me_start, s->n_effects);
    EQ("enemy items", run.n_enemy_items, s->n_items);
    for (int e = 0; e < s->n_x && e < 6 && e < run.n_enemy; e++) {
        const gc_mon *m = &run.enemy[e];
        const mon_row *up = &s->mon[e];
        enemies_checked++;
        char nm[48];
        EQ("species", m->b.species, up->species);
        EQ("form", m->b.form, up->form);
        EQ("level", m->b.level, up->level);
        EQ("level is the battle's", m->b.level, s->levels[e]);
        EQ("boss", m->b.is_boss, up->segments ? 1 : 0);
        EQ("segments", m->b.boss_segments, up->segments);
        for (int j = 0; j < 6; j++) { snprintf(nm, sizeof nm, "enemy %d iv %d", e, j); EQ(nm, m->b.ivs[j], up->ivs[j]); }
        EQ("nature", m->has_custom_nature ? m->base_nature : m->b.nature, up->nature);
        EQ("ability index", m->ability_index, up->ability);
        EQ("passive flag", m->b.passive_flag, up->passive);
        EQ("shiny", m->b.shiny, up->shiny);
        EQ("variant", m->variant, up->variant);
        EQ("gender", m->b.gender, up->gender);
        EQ("friendship", m->b.friendship, up->friendship);
        EQ("id", m->b.id, up->id);
        EQ("max hp", m->b.stats[0], up->stats[0]);
        EQ("hp", m->b.hp, up->hp);
        hp_checked++;
        for (int j = 0; j < 6; j++) { snprintf(nm, sizeof nm, "enemy %d stat %d", e, j); EQ(nm, m->b.stats[j], up->stats[j]); }
        EQ("move count", m->b.n_moves, up->n_moves);
        for (int j = 0; j < up->n_moves && j < 4; j++) { snprintf(nm, sizeof nm, "enemy %d move %d", e, j); EQ(nm, m->b.moves[j], up->moves[j]); moves_checked++; }
        EQ("pokeball", m->pokeball, s->x3[e].ball);
        EQ("tera type", m->b.tera_type, s->x3[e].tera);
        EQ("ability", pr_get_ability(&m->b, false), s->x3[e].ability);
        EQ("luck", m->luck, s->x3[e].luck);
        EQ("trainer slot", m->trainer_slot, s->x3[e].slot);
        EQ("fusion", m->b.fusion_species, s->x3[e].fusion);
        forms += up->form > 0;
    }
    for (int j = 0; j < s->n_items; j++) {
        bool found = false;
        for (int i = 0; i < run.n_enemy_items; i++) {
            const gc_item *it = &run.enemy_items[i];
            found = found || (!strcmp(it->key, s->items[j].key) && it->arg == s->items[j].arg && it->stack == s->items[j].stack
                              && s->items[j].holder >= 0 && s->items[j].holder < s->n_x && it->pokemon_id == run.enemy[s->items[j].holder].b.id
                              && (it->non_transferable ? 0 : 1) == s->items[j].transferable);
        }
        CHECK(found, "enemy lacks item %s/%d x%d on %d (transferable %d)", s->items[j].key, s->items[j].arg, s->items[j].stack, s->items[j].holder, s->items[j].transferable);
        items_checked++;
    }
    /* the trainer's own line comes first among the events; the callbacks of the player's Pokemon coming back are one Recall each */
    int recalls = 0, dialogue = 0, trainer_ev = 0;
    char want[64];
    snprintf(want, sizeof want, "dialogue:winstrates%s.encounter.1", tname[k]);
    gc_event e;
    while (gc_run_next_event(&run, &e)) {
        if (e.kind == GC_EV_RECALL) recalls++;
        if (e.kind == GC_EV_BATTLE) {   /* the outgoing field's cleanup: battle-core's switch-out events, one for each Pokemon of the player's on the field */
            const pr_event *pe = gc_run_battle_event_data(&run) + e.a;
            for (int i = 0; i < e.b; i++) if (pe[i].type == PR_EV_SWITCH_OUT && pe[i].battler >= 0 && pe[i].battler < 2) recalls++;
        }
        if (e.kind == GC_EV_TRAINER) { trainer_ev++; EQ("trainer event type", e.a, run.trainer_type); EQ("trainer event variant", e.b, run.trainer_variant); }
        if (e.kind == GC_EV_DIALOGUE && e.msg.key && !strcmp(e.msg.key, want)) dialogue++;
    }
    EQ("Pokemon called back", recalls, s->n_returns);
    EQ("the trainer shown", trainer_ev, 1);
    EQ("the trainer's line", dialogue, 1);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_winstrate_diff_test <trace>\n", stderr); return 2; }
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, refusals = 0, chains = 0, waves[8] = {0}, rewards_checked = 0;
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            n_mons = (int)integer();
            if (n_mons < 3 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            for (int m = 0; m < n_mons * 2; m++) integer();
            continue;
        }
        if (strcmp(w, "C") || !n_mons) { fprintf(stderr, "wrong record %s\n", w); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        case_wave = wave; step_no = 0;
        struct { int species, form, level, luck; } setup[6];
        tag("U");
        for (int m = 0; m < n_mons; m++) {
            if (m) semicolon();
            setup[m].species = (int)integer(); setup[m].form = (int)integer(); setup[m].level = (int)integer(); setup[m].luck = (int)integer();
        }
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("K");
        const int n_configs = (int)integer();
        { char c[96]; word(c, sizeof c); }   /* trainerType:members, comma-separated */
        tag("O");
        const int refuse = (int)integer();
        step_t s;
        read_step("Q", &s);

        int st[6];
        for (int m = 0; m < n_mons; m++) st[m] = (m + case_no % n_mons) % n_mons;
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
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            mon->b.id = (uint32_t)(0x4000 + m);
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            mon->b.level = (uint8_t)setup[m].level;
            mon->luck = (int8_t)setup[m].luck;
            mon->b.shiny = setup[m].luck > 0;
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        gc_items_to_battle(&run);
        bad = 0;
        run.trainer_id = trainer_id;
        run.secret_id = secret_id;
        run.money = money_before;
        run.wave = wave;
        run.biome = biome;
        run.n_enemy = 0;
        run.in_battle = false;
        run.me_battle = false;
        run.battle_type = BATTLE_TYPE_WILD;
        pr_seed_from_ascii(&run.run.wave_seed, wave_seed);
        run.run.rng = before;
        gc_rq_reset(&gc_int(&run)->rq);
        run.mystery_type = GC_ME_THE_WINSTRATE_CHALLENGE;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_THE_WINSTRATE_CHALLENGE;
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        gc_mystery_setup_screen(&run);
        CHECK(run.screen.n_options == 2 && run.screen.option_ok[0], "%d options, the challenge %d", run.screen.n_options, run.screen.option_ok[0]);
        CHECK(same_rng(&run.run.rng, &after_init), "onInit moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);
        EQ("five trainer configs", n_configs, 5);

        bool ok = false;
        const char *w2 = "";
        gc_mystery_execute(&run, refuse, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &s.rng), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, s.rng.c);
        if (refuse) {
            tag("T");
            const int calls = (int)integer(), fill = (int)integer(), n_funcs = (int)integer();
            char fn[48] = "";
            for (int k = 0; k < n_funcs; k++) word(fn, sizeof fn);
            const int n_tiers = (int)integer(), n_options = (int)integer(), n_leaves = (int)integer();
            char heal[8], mode[24];
            word(heal, sizeof heal); word(mode, sizeof mode);
            tag("E");
            const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
            EQ("rewards set once", calls, 1);
            EQ("rewards fill the rest", c->fill_remaining, fill);
            EQ("one reward function", c->n_funcs, n_funcs);
            EQ("the reward function", c->n_funcs ? c->funcs[0] : -1, gc_reward_item_index(fn));
            EQ("no reward tiers", c->n_tiers, n_tiers);
            EQ("no reward options", c->n_options, n_options);
            EQ("left once", n_leaves, 1);
            EQ("left without the heal-only shop", atoi(heal), 0);
            CHECK(gc_rq_has(&run, RP_PARTY_HEAL), "no PartyHealPhase queued");
            CHECK(!run.in_battle && !gc_int(&run)->winstrate.active, "a fight started");
            EQ("no fight", s.fight, 0);
            rewards_checked++;
            refusals++;
        } else {
            step_no = 0;
            compare_fight(&s, 0);
            CHECK(gc_int(&run)->winstrate.active, "the chain is not running");
            for (int k = 1; k <= 5; k++) {
                char label[4];
                snprintf(label, sizeof label, "Q%d", k);
                read_step(label, &s);
                step_no = k;
                { gc_event e; while (gc_run_next_event(&run, &e)) {} }
                gc_rq_reset(&gc_int(&run)->rq);
                run.in_battle = false;
                gc_winstrate_next(&run);
                CHECK(same_rng(&run.run.rng, &s.rng), "the run RNG after step %d differs (c %.0f, upstream %.0f)", k, run.run.rng.c, s.rng.c);
                if (k < 5) {
                    compare_fight(&s, k);
                    continue;
                }
                /* the end: the victory line, the voucher, the second line, a shop of one Macho Brace */
                static const char *const want[3] = {"mysteryEncounters/theWinstrateChallenge:victory", "battle:rewardGain", "mysteryEncounters/theWinstrateChallenge:victory2"};
                EQ("three messages said", s.n_said, 3);
                int got = 0;
                gc_event e;
                while (gc_run_next_event(&run, &e)) {
                    if (e.kind != GC_EV_MESSAGE || !e.msg.key) continue;
                    if (got < 3) { fields_checked++; CHECK(!strcmp(e.msg.key, want[got]), "message %d is %s, upstream %s", got, e.msg.key, want[got]); }
                    got++;
                }
                EQ("messages said", got, 3);
                CHECK(s.n_said == 3 && !strncmp(s.said[0], "D:mysteryEncounters/theWinstrateChallenge:victory@", 49) && !strncmp(s.said[1], "T:You_received_1", 16)
                      && !strncmp(s.said[2], "D:mysteryEncounters/theWinstrateChallenge:victory2@", 50), "upstream said %s / %s / %s", s.said[0], s.said[1], s.said[2]);
                tag("T");
                const int calls = (int)integer(), fill = (int)integer(), n_funcs = (int)integer(), n_tiers = (int)integer(), n_options = (int)integer();
                char oid[48] = "";
                int otier = 0, oup = 0, ocost = 0;
                for (int j = 0; j < n_options; j++) { word(oid, sizeof oid); otier = (int)integer(); oup = (int)integer(); ocost = (int)integer(); }
                const int n_leaves = (int)integer();
                char heal[8], mode[24];
                word(heal, sizeof heal); word(mode, sizeof mode);
                const int cont = (int)integer();
                tag("B");
                char before_v[48], after_v[48], mods_n[8];
                word(before_v, sizeof before_v); word(after_v, sizeof after_v); word(mods_n, sizeof mods_n);
                if (atoi(mods_n) > 0) { char md[1024]; word(md, sizeof md); }   /* the held items, comma-separated */
                tag("E");
                const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
                EQ("rewards set once", calls, 1);
                EQ("rewards fill the rest", c->fill_remaining, fill);
                EQ("no reward functions", c->n_funcs, n_funcs);
                EQ("no reward tiers", c->n_tiers, n_tiers);
                EQ("one reward option", c->n_options, n_options);
                if (c->n_options == 1 && n_options == 1) {
                    EQ("the reward is the Macho Brace", c->options[0].type.item, PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE);
                    CHECK(!strcmp(oid, "MYSTERY_ENCOUNTER_MACHO_BRACE"), "upstream's reward is %s", oid);
                    EQ("its tier", c->options[0].tier, otier);
                    EQ("its tier is MASTER", c->options[0].tier, MODIFIER_TIER_MASTER);
                    EQ("its upgrade count", c->options[0].upgrade_count, oup);
                    EQ("its cost", (long long)c->options[0].cost, ocost);
                }
                EQ("left once", n_leaves, 1);
                EQ("left without the heal-only shop", atoi(heal), 0);
                EQ("doContinueEncounter cleared", cont, 0);
                EQ("the chain is over", gc_int(&run)->winstrate.active, 0);
                /* the voucher: +1 Premium, nothing else */
                uint32_t counts[4] = {0};
                int vb[4] = {0}, va[4] = {0};
                sscanf(before_v, "%d,%d,%d,%d", &vb[0], &vb[1], &vb[2], &vb[3]);
                sscanf(after_v, "%d,%d,%d,%d", &va[0], &va[1], &va[2], &va[3]);
                for (int j = 0; j < 4; j++) { counts[j] = sys.voucher_counts[j]; EQ("voucher count gained", counts[j], va[j] - vb[j]); }
                EQ("a Premium voucher", va[2] - vb[2], 1);
                CHECK(gc_rq_has(&run, RP_SELECT_MODIFIER), "no shop queued");
                rewards_checked++;
            }
            chains++;
        }
        waves[(wave - 100) / 10 > 7 ? 7 : (wave - 100) / 10]++;
        if (bad) mismatches++;
        cases++;
    }
    printf("Mystery encounter The Winstrate Challenge: %d cases run (%d chains of five fights, %d refusals; waves 100s..170s %d/%d/%d/%d/%d/%d/%d/%d), %d fights, %d enemies compared (%d with a form, %d hp), %d items and %d moves compared, %d female trainers, %d rewards, %d fields. TOTAL mismatches %d\n",
           cases, chains, refusals, waves[0], waves[1], waves[2], waves[3], waves[4], waves[5], waves[6], waves[7], fights, enemies_checked, forms, hp_checked, items_checked, moves_checked, females, rewards_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
