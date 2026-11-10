/* Weird Dream against upstream at 8555c08: replays the "-weird" trace of tests/mystery-weird.producer.test.ts (the real encounter: onInit in executeWithSeedOffset(.., waveIndex), the
 * option's onPreOptionPhase in (.., getSeedOffset()) and onOptionPhase in (.., getSeedOffset() * 500) really run; the animations, the dialogue and the dream background are stubbed;
 * setEncounterRewards, leaveEncounterWithoutBattle and the queued PartyHealPhase recorded) through game-core: the species list in registry order, onInit's transformations (species,
 * the new Pokemon the constructor made and their Tera types), the run RNG around the option, the party after (every field), the items, the dex entries of the new species' lines,
 * the rewards and the dialogue keys.
 * Usage: mystery_weird_diff_test <trace>; exit 0 only with 0 mismatches. */
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
    if (strcmp(s, ";")) { fprintf(stderr, "wrong record: wanted ;, got %s\n", s); exit(2); }
}
static void bar(void)
{
    char s[4];
    word(s, sizeof s);
    if (strcmp(s, "|")) { fprintf(stderr, "wrong record: wanted |, got %s (byte %ld)\n", s, ftell(f)); exit(2); }
}
static pr_rng rng_vals(void)
{
    pr_rng r = {0};
    r.c = real(); r.s0 = real(); r.s1 = real(); r.s2 = real();
    return r;
}
static bool same_rng(const pr_rng *a, const pr_rng *b) { return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2; }

static int bad, shown, case_no, case_option, case_wave;
#define CHECK(cond, ...) do { if (!(cond)) { bad++; if (shown < (getenv("SHOW") ? atoi(getenv("SHOW")) : 14)) { shown++; fprintf(stderr, "case %d (option %d wave %d): ", case_no, case_option, case_wave); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
static int fields_checked;
#define EQ(name, c_val, up_val) do { fields_checked++; CHECK((long long)(c_val) == (long long)(up_val), "%s is %lld, upstream %lld", name, (long long)(c_val), (long long)(up_val)); } while (0)

typedef struct { char key[48]; int arg, holder, stack, virt; } item_row;
static int read_items(item_row *rows)
{
    const int n = (int)integer();
    if (n < 0 || n > 160) { fputs("item count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        word(rows[k].key, sizeof rows[k].key);
        rows[k].arg = (int)integer();
        rows[k].holder = (int)integer();
        rows[k].stack = (int)integer();
        rows[k].virt = (int)integer();
    }
    return n;
}

/* "-1,5" / "-" / "9,1": a comma list from the trace */
static int csv(char *w, int *out)
{
    int n = 0;
    if (!strcmp(w, "-")) return 0;
    for (char *p = w; *p;) { out[n++] = atoi(p); while (*p && *p != ',') p++; if (*p == ',') p++; }
    return n;
}

/* a Pokemon as the producer's dump() writes it */
typedef struct {
    int species, form, level, nature, ivs[6], ability_index, passive, shiny, variant, gender, friendship, tera, luck, pokerus, hp, max, stats[6];
    long long exp;
    int ability, status, toxic_turns, sleep_turns, freeze_turns, n_custom, custom[2], n_types, types[3], n_moves, moves[4], pp_used[4], pp_up[4];
} dump_row;
static void read_dump(dump_row *d)
{
    d->species = (int)integer(); d->form = (int)integer(); d->level = (int)integer(); d->nature = (int)integer();
    for (int k = 0; k < 6; k++) d->ivs[k] = (int)integer();
    d->ability_index = (int)integer(); d->passive = (int)integer(); d->shiny = (int)integer(); d->variant = (int)integer(); d->gender = (int)integer();
    d->friendship = (int)integer(); d->tera = (int)integer(); d->luck = (int)integer(); d->pokerus = (int)integer();
    d->hp = (int)integer(); d->max = (int)integer();
    for (int k = 0; k < 6; k++) d->stats[k] = (int)integer();
    d->exp = integer(); d->ability = (int)integer(); d->status = (int)integer();
    d->toxic_turns = (int)integer(); d->sleep_turns = (int)integer(); d->freeze_turns = (int)integer();
    char a[32], b[32];
    word(a, sizeof a); word(b, sizeof b);
    d->n_custom = csv(a, d->custom);
    d->n_types = csv(b, d->types);
    d->n_moves = (int)integer();
    for (int k = 0; k < d->n_moves; k++) {
        const int mv = (int)integer(), used = (int)integer(), up = (int)integer();
        if (k < 4) { d->moves[k] = mv; d->pp_used[k] = used; d->pp_up[k] = up; }
    }
}

static int compare_mon(const char *what, const gc_mon *m, const dump_row *d)
{
    char nm[96];
    const pr_pokemon *p = &m->b;
#define F(field, c_val, up_val) do { snprintf(nm, sizeof nm, "%s %s", what, field); EQ(nm, c_val, up_val); } while (0)
    F("species", p->species, d->species);
    F("form", p->form, d->form);
    F("level", p->level, d->level);
    F("nature", p->nature, d->nature);
    for (int k = 0; k < 6; k++) { char fn[16]; snprintf(fn, sizeof fn, "iv %d", k); F(fn, p->ivs[k], d->ivs[k]); }
    F("ability index", m->ability_index, d->ability_index);
    F("passive", p->passive_flag, d->passive);
    F("shiny", p->shiny, d->shiny);
    F("variant", m->variant, d->variant);
    F("gender", p->gender, d->gender);
    F("friendship", p->friendship, d->friendship);
    F("tera type", p->tera_type, d->tera);
    F("luck", m->luck, d->luck);
    F("pokerus", m->pokerus, d->pokerus);
    F("hp", p->hp, d->hp);
    F("max hp", p->stats[0], d->max);
    for (int k = 0; k < 6; k++) { char fn[16]; snprintf(fn, sizeof fn, "stat %d", k); F(fn, p->stats[k], d->stats[k]); }
    F("exp", (long long)m->exp, d->exp);
    F("ability", pr_get_ability(p, false), d->ability);
    F("status", p->status, d->status);
    F("status toxic turns", p->status_toxic_turns, d->toxic_turns);
    F("status sleep turns", p->status_sleep_turns < 0 ? 0 : p->status_sleep_turns, d->sleep_turns);   /* -1 is undefined, dumped as 0 */
    F("status freeze turns", p->status_freeze_turns < 0 ? 0 : p->status_freeze_turns, d->freeze_turns);
    int custom = 0;
    if (p->custom_types[0] || p->custom_types[1]) {
        custom = 2;
        F("custom type 0", p->custom_types[0] ? p->custom_types[0] - 1 : -1, d->n_custom > 0 ? d->custom[0] : -2);
        F("custom type 1", p->custom_types[1] ? p->custom_types[1] - 1 : -1, d->n_custom > 1 ? d->custom[1] : -2);
    }
    F("custom type count", custom, d->n_custom);
    int8_t types[PR_MAX_TYPES];
    const int n_types = pr_get_types(p, types, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
    F("type count", n_types, d->n_types);
    for (int k = 0; k < n_types && k < d->n_types; k++) { char fn[16]; snprintf(fn, sizeof fn, "type %d", k); F(fn, types[k], d->types[k]); }
    F("move count", p->n_moves, d->n_moves);
    for (int k = 0; k < p->n_moves && k < d->n_moves && k < 4; k++) {
        char fn[24];
        snprintf(fn, sizeof fn, "move %d", k); F(fn, p->moves[k], d->moves[k]);
        snprintf(fn, sizeof fn, "move %d PP used", k); F(fn, p->pp_used[k], d->pp_used[k]);
        snprintf(fn, sizeof fn, "move %d PP up", k); F(fn, p->pp_up[k], d->pp_up[k]);
    }
#undef F
    return 0;
}

typedef struct { long long id; long long caught; long long nature; long long caught_count, hatched, seen; int ivs[6]; long long candy, ability, egg; } dex_row;
static int read_dex(dex_row *rows)
{
    const int n = (int)integer();
    if (n < 0 || n > 40) { fputs("dex count\n", stderr); exit(2); }
    for (int k = 0; k < n; k++) {
        char w[160];
        word(w, sizeof w);
        char *c = w;
        long long v[16];
        int nv = 0;
        /* id:caught:nature:count:hatched:seen:iv,iv,iv,iv,iv,iv:candy:ability:eggs */
        while (nv < 16 && *c) {
            v[nv++] = atoll(c);
            while (*c && *c != ':' && *c != ',') c++;
            if (*c) c++;
        }
        if (nv != 15) { fprintf(stderr, "dex entry %s has %d values\n", w, nv); exit(2); }
        rows[k].id = v[0]; rows[k].caught = v[1]; rows[k].nature = v[2]; rows[k].caught_count = v[3]; rows[k].hatched = v[4]; rows[k].seen = v[5];
        for (int i = 0; i < 6; i++) rows[k].ivs[i] = (int)v[6 + i];
        rows[k].candy = v[12]; rows[k].ability = v[13]; rows[k].egg = v[14];
    }
    return n;
}

static void check_dex(const char *what, const dex_row *rows, int n)
{
    char nm[96];
    for (int k = 0; k < n; k++) {
        const int id = (int)rows[k].id;
        const gc_dex_entry *d = &sys.dex[id];
        snprintf(nm, sizeof nm, "%s dex %d caught attr", what, id); EQ(nm, (long long)d->caught_attr, rows[k].caught);
        snprintf(nm, sizeof nm, "%s dex %d nature attr", what, id); EQ(nm, (long long)d->nature_attr, rows[k].nature);
        snprintf(nm, sizeof nm, "%s dex %d caught count", what, id); EQ(nm, (long long)d->caught_count, rows[k].caught_count);
        snprintf(nm, sizeof nm, "%s dex %d hatched count", what, id); EQ(nm, (long long)d->hatched_count, rows[k].hatched);
        snprintf(nm, sizeof nm, "%s dex %d seen attr", what, id); EQ(nm, (long long)d->seen_attr, rows[k].seen);
        for (int i = 0; i < 6; i++) { snprintf(nm, sizeof nm, "%s dex %d iv %d", what, id, i); EQ(nm, d->ivs[i], rows[k].ivs[i]); }
        if (rows[k].candy != -1) {
            const gc_starter_entry *s = &sys.starters[id];
            snprintf(nm, sizeof nm, "%s starter %d candy", what, id); EQ(nm, (long long)s->candy_count, rows[k].candy);
            snprintf(nm, sizeof nm, "%s starter %d ability attr", what, id); EQ(nm, (long long)s->ability_attr, rows[k].ability);
            snprintf(nm, sizeof nm, "%s starter %d egg moves", what, id); EQ(nm, (long long)s->egg_moves, rows[k].egg);
        }
    }
}

static int holder_of(const gc_item *it, const uint32_t *old_ids, int n_old)
{
    if (!it->pokemon_id) return -1;
    for (int k = 0; k < run.n_party; k++) if (run.party[run.party_order[k]].b.id == it->pokemon_id) return k;
    for (int k = 0; k < n_old; k++) if (old_ids[k] == it->pokemon_id) return -3 - k;
    return -2;
}

static void check_items(const char *what, const item_row *rows, int n, const uint32_t *old_ids, int n_old)
{
    char nm[96];
    snprintf(nm, sizeof nm, "%s: item count", what);
    EQ(nm, run.n_items, n);
    for (int k = 0; k < n && k < run.n_items; k++) {
        const gc_item *it = &run.items[k];
        const int holder = holder_of(it, old_ids, n_old);
        CHECK(!strcmp(it->key, rows[k].key) && it->arg == rows[k].arg && holder == rows[k].holder && it->stack == rows[k].stack && it->virtual_stack == rows[k].virt,
              "%s: item %d is %s/%d holder %d x%d (+%d), upstream %s/%d holder %d x%d (+%d)", what, k, it->key, it->arg, holder, it->stack, it->virtual_stack,
              rows[k].key, rows[k].arg, rows[k].holder, rows[k].stack, rows[k].virt);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !(f = fopen(argv[1], "r"))) { fputs("usage: mystery_weird_diff_test <trace>\n", stderr); return 2; }
    gc_weird_reset_ranges();
    char w[16];
    word(w, sizeof w);
    if (strcmp(w, "V")) return 2;
    integer();
    integer();
    int n_mons = 0;
    int cases = 0, mismatches = 0, by_option[3] = {0}, transformations = 0, starters_said = 0, new_species_seen = 0, listed = 0, over600 = 0, items_checked = 0, dex_checked = 0;
    int seen_species[8192] = {0};
    int first_in_config = 0, searched = 0;
    for (;;) {
        word(w, sizeof w);
        if (!strcmp(w, "Z")) break;
        if (!strcmp(w, "P")) {
            n_mons = (int)integer();
            if (n_mons < 3 || n_mons > 6) { fputs("party size\n", stderr); return 2; }
            first_in_config = 1;
            continue;
        }
        if (!strcmp(w, "S")) {
            /* the registry's species in order, with their base stat totals */
            const int n = (int)integer();
            bad = 0; case_no = -1; case_option = -1; case_wave = -1;
            EQ("species count", PR_SPECIES_COUNT, n);
            for (int k = 0; k < n; k++) {
                char tok[32];
                word(tok, sizeof tok);
                const int id = atoi(tok), bst = atoi(strchr(tok, ':') + 1);
                CHECK(k < PR_SPECIES_COUNT && pr_species_table[k].id == id, "species %d is %d, upstream %d", k, k < PR_SPECIES_COUNT ? pr_species_table[k].id : -1, id);
                const pr_form *fm = gc_form(id, 0);
                CHECK(fm && fm->base_total == bst, "species %d base stat total is %d, upstream %d", id, fm ? fm->base_total : -1, bst);
                CHECK(gc_species_gen(id) != NULL, "species %d has no generation data", id);
                listed++;
            }
            mismatches += bad != 0;
            continue;
        }
        if (strcmp(w, "C") || !n_mons) { fprintf(stderr, "wrong record %s\n", w); return 2; }
        char run_seed[64], wave_seed[64];
        case_no = (int)integer();
        word(run_seed, sizeof run_seed); word(wave_seed, sizeof wave_seed);
        const int wave = (int)integer(), option = (int)integer(), biome = (int)integer();
        const int trainer_id = (int)integer(), secret_id = (int)integer();
        const double money_before = (double)integer();
        case_option = option; case_wave = wave;
        static dump_row setup[6], tf[6], after[6];
        tag("U");
        for (int m = 0; m < n_mons; m++) { if (m) semicolon(); read_dump(&setup[m]); }
        static item_row before_items[160], after_items[160];
        tag("L");
        const int n_before = read_items(before_items);
        tag("R");
        const pr_rng before = rng_vals();
        tag("I");
        const pr_rng after_init = rng_vals();
        tag("TF");
        const int n_tf = (int)integer();
        int tf_prev[6], tf_species[6], tf_held[6];
        for (int m = 0; m < n_tf && m < 6; m++) {
            if (m) semicolon();
            tf_prev[m] = (int)integer(); tf_species[m] = (int)integer(); tf_held[m] = (int)integer();
            bar();
            read_dump(&tf[m]);
        }
        static dex_row dex_before[40], dex_after[40];
        tag("DB");
        const int n_dex = read_dex(dex_before);
        tag("Q");
        rng_vals();
        rng_vals();
        const pr_rng after_option = rng_vals();
        tag("T");
        const int n_rewards = (int)integer(), leave_calls = (int)integer(), leave_arg = (int)integer(), rw_fill = (int)integer(), n_funcs = (int)integer();
        char funcs[8][32] = {{0}};
        for (int k = 0; k < n_funcs; k++) { char fn[32]; word(fn, sizeof fn); if (k < 8) snprintf(funcs[k], sizeof funcs[k], "%s", fn); }
        tag("QU");
        const int n_queued = (int)integer();
        char queued[8][40] = {{0}};
        for (int k = 0; k < n_queued; k++) { char q[40]; word(q, sizeof q); if (k < 8) snprintf(queued[k], sizeof queued[k], "%s", q); }
        tag("DL");
        const int n_dialogue = (int)integer();
        char dialogue[16][64] = {{0}};
        for (int k = 0; k < n_dialogue; k++) { char d[64]; word(d, sizeof d); if (k < 16) snprintf(dialogue[k], sizeof dialogue[k], "%s", d); }
        tag("F");
        const int n_after_party = (int)integer();
        for (int m = 0; m < n_after_party && m < 6; m++) { if (m) semicolon(); read_dump(&after[m]); }
        tag("V");
        const int n_after = read_items(after_items);
        tag("DA");
        const int n_dex_after = read_dex(dex_after);
        tag("AC");
        { char a[160]; word(a, sizeof a); while (1) { int c = fgetc(f); if (c == '\n' || c == EOF) break; } }
        /* option 2 (the fight): the rewards given, the trainer, the enemy items, the enemy party, their extras, the data sources, and the win's passive callback */
        int fr_n = 0, fr_fill = 0, fr_funcs = 0, fr_cb = 0, fr_tiers[8] = {0}, fr_n_tiers = 0;
        int fd_mode = 0, fd_double = 0, fd_levels = 0, fd_trainer = 0, fd_variant = 0;
        char fd_name[64] = "";
        int fj_n = 0;
        static struct { int holder, arg, stack, transferable; char key[48]; } fj[160];
        int fx_n = 0;
        static struct { int boss, segments; long long id; int slot, ai, populated, pokerus, luck, custom_ability; } fx[6];
        static dump_row fxd[6], ftg[6];
        int fx3_n = 0;
        static struct { int pokeball, tera, ability; char name[40]; } fx3[6];
        int ftg_n = 0;
        pr_rng br_before = {0}, br_after = {0};
        int br_passives[6] = {0};
        if (option == 1) {
            tag("RW");
            fr_n = (int)integer(); fr_fill = (int)integer();
            { char t[40]; word(t, sizeof t); fr_n_tiers = csv(t, fr_tiers); }
            fr_funcs = (int)integer(); fr_cb = (int)integer();
            tag("D");
            fd_mode = (int)integer(); fd_double = (int)integer(); fd_levels = (int)integer();
            for (int k = 0; k < fd_levels; k++) integer();
            fd_trainer = (int)integer(); fd_variant = (int)integer();
            word(fd_name, sizeof fd_name);
            tag("J");
            fj_n = (int)integer();
            if (fj_n < 0 || fj_n > 160) { fputs("enemy item count\n", stderr); return 2; }
            for (int k = 0; k < fj_n; k++) {
                fj[k].holder = (int)integer();
                word(fj[k].key, sizeof fj[k].key);
                fj[k].arg = (int)integer(); fj[k].stack = (int)integer(); fj[k].transferable = (int)integer();
            }
            tag("X");
            fx_n = (int)integer();
            for (int k = 0; k < fx_n && k < 6; k++) {
                if (k) semicolon();
                fx[k].boss = (int)integer(); fx[k].segments = (int)integer(); fx[k].id = integer(); fx[k].slot = (int)integer(); fx[k].ai = (int)integer();
                fx[k].populated = (int)integer(); fx[k].pokerus = (int)integer(); fx[k].luck = (int)integer(); fx[k].custom_ability = (int)integer();
                bar();
                read_dump(&fxd[k]);
            }
            tag("X3");
            fx3_n = (int)integer();
            for (int k = 0; k < fx3_n && k < 6; k++) {
                if (k) semicolon();
                fx3[k].pokeball = (int)integer(); fx3[k].tera = (int)integer(); fx3[k].ability = (int)integer();
                word(fx3[k].name, sizeof fx3[k].name);
            }
            tag("TG");
            ftg_n = (int)integer();
            for (int k = 0; k < ftg_n && k < 6; k++) { if (k) semicolon(); read_dump(&ftg[k]); }
            tag("BR");
            br_before = rng_vals(); br_after = rng_vals();
            for (int k = 0; k < n_mons; k++) br_passives[k] = (int)integer();
        }
        tag("E");
        (void)n_dex_after;

        bool win = false;
        /* the storage order is rotated, so that party order and storage order differ */
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
        uint32_t old_ids[6];
        for (int m = 0; m < n_mons; m++) {
            run.party[st[m]] = tmpl;
            gc_mon *mon = &run.party[st[m]];
            old_ids[m] = (uint32_t)(0x4000 + m);
            mon->b.id = old_ids[m];
            mon->b.species = (uint16_t)setup[m].species;
            mon->b.form = (uint8_t)setup[m].form;
            mon->b.fusion_species = 0;
            memset(mon->b.custom_types, 0, sizeof mon->b.custom_types);
            memset(mon->b.fusion_custom_types, 0, sizeof mon->b.fusion_custom_types);
            mon->b.level = setup[m].level;
            mon->b.status = (uint8_t)setup[m].status;
            mon->b.status_toxic_turns = (int16_t)setup[m].toxic_turns;
            mon->b.status_sleep_turns = (int16_t)setup[m].sleep_turns;
            mon->b.status_freeze_turns = (int16_t)setup[m].freeze_turns;
            mon->b.n_moves = (uint8_t)setup[m].n_moves;
            memset(mon->b.moves, 0, sizeof mon->b.moves);
            memset(mon->b.pp_used, 0, sizeof mon->b.pp_used);
            memset(mon->b.pp_up, 0, sizeof mon->b.pp_up);
            for (int k = 0; k < setup[m].n_moves; k++) { mon->b.moves[k] = (uint16_t)setup[m].moves[k]; mon->b.pp_used[k] = (uint8_t)setup[m].pp_used[k]; mon->b.pp_up[k] = (int8_t)setup[m].pp_up[k]; }
            for (int k = 0; k < 6; k++) mon->b.ivs[k] = (uint8_t)setup[m].ivs[k];
            mon->base_nature = (uint8_t)setup[m].nature;
            mon->has_custom_nature = false;
            mon->b.nature = (uint8_t)setup[m].nature;
            mon->ability_index = (uint8_t)setup[m].ability_index;
            mon->b.ability_index = (int8_t)setup[m].ability_index;
            mon->b.gender = (int8_t)setup[m].gender;
            mon->b.friendship = (uint8_t)setup[m].friendship;
            mon->b.tera_type = (int8_t)setup[m].tera;
            mon->b.shiny = setup[m].shiny != 0;
            mon->variant = (uint8_t)setup[m].variant;
            mon->luck = (int8_t)setup[m].luck;
            mon->pokerus = setup[m].pokerus != 0;
            mon->b.passive_flag = setup[m].passive != 0;
            mon->exp = (double)setup[m].exp;
            mon->b.ability = mon->b.base_ability = (uint16_t)setup[m].ability;
            mon->b.custom_ability = (int16_t)setup[m].ability;   /* the producer gives every member customPokemonData.ability */
            mon->b.on_field = false;
            run.party_order[m] = (int8_t)st[m];
        }
        run.n_party = n_mons;
        for (int k = 0; k < n_before; k++) {
            const int idx = gc_reward_item_index(before_items[k].key);
            if (idx < 0) { fprintf(stderr, "unknown item %s\n", before_items[k].key); return 2; }
            gc_item *it = &run.items[run.n_items++];
            it->key = pr_item_keys[idx];
            it->arg = (int16_t)before_items[k].arg;
            it->pokemon_id = before_items[k].holder < 0 ? 0 : (uint32_t)(0x4000 + before_items[k].holder);
            it->stack = before_items[k].stack;
            it->virtual_stack = before_items[k].virt;
            it->non_transferable = gc_item_key_locked(it->key);
        }
        gc_items_to_battle(&run);
        bad = 0;
        for (int m = 0; m < n_mons; m++) {
            gc_mon *mon = &run.party[st[m]];
            pr_update_passive(&run.bt, &mon->b);
            pr_calculate_stats(&run.bt, &mon->b);
            bool same = true;
            for (int k = 0; k < 6; k++) same = same && mon->b.stats[k] == setup[m].stats[k];
            CHECK(same && mon->b.stats[0] == setup[m].max, "starting stats of Pokemon %d differ (C max %d, upstream %d)", m, mon->b.stats[0], setup[m].max);
            mon->b.hp = setup[m].hp;
        }
        run.bt.n_party[0] = (uint8_t)run.n_party;
        for (int k = 0; k < run.n_party; k++) run.bt.party[0][k] = &run.party[run.party_order[k]].b;
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
        run.mystery_type = GC_ME_WEIRD_DREAM;
        memset(&run.screen, 0, sizeof run.screen);
        run.screen.kind = GC_SCREEN_MYSTERY;
        run.screen.mystery_type = GC_ME_WEIRD_DREAM;
        gc_mystery_record(&run, GC_ME_WEIRD_DREAM, GC_ME_ROGUE);
        { gc_event e; while (gc_run_next_event(&run, &e)) {} }
        /* the dex entries of the new species' lines, part-filled */
        for (int k = 0; k < n_dex; k++) {
            const int id = (int)dex_before[k].id;
            gc_dex_entry *d = &sys.dex[id];
            d->caught_attr = (uint64_t)dex_before[k].caught;
            d->nature_attr = (uint32_t)dex_before[k].nature;
            d->caught_count = (uint32_t)dex_before[k].caught_count;
            d->hatched_count = (uint32_t)dex_before[k].hatched;
            d->seen_attr = (uint64_t)dex_before[k].seen;
            for (int i = 0; i < 6; i++) d->ivs[i] = (uint8_t)dex_before[k].ivs[i];
            if (dex_before[k].candy != -1) {
                gc_starter_entry *s = &sys.starters[id];
                s->candy_count = (int32_t)dex_before[k].candy;
                s->ability_attr = (uint32_t)dex_before[k].ability;
                s->egg_moves = (uint8_t)dex_before[k].egg;
            }
        }
        gc_mystery_setup_screen(&run);
        if (first_in_config) {
            /* The module-level ranges upstream widens in place have a history this trace does not hold (the run before this configuration's first case started the real
             * encounter once, and every earlier case may have widened them): the widening steps in force are searched for, as the smallest pair, not under the ones
             * the cases before left, that makes game-core choose upstream's species. Every later case then has to follow on from there. */
            first_in_config = 0;
            int ha, sa;
            gc_weird_get_ranges(&ha, &sa);
            ha = sa = 0;   /* absolute: the history is searched from the untouched ranges, the pair the cases before left may be wider than upstream's */
            bool found = false;
            for (int total = 0; total <= 60 && !found; total++) {
                for (int a = 0; a <= total && !found; a++) {
                    const int b = total - a;
                    gc_weird_set_ranges(ha + a, sa + b);
                    gc_mystery_record(&run, GC_ME_WEIRD_DREAM, GC_ME_ROGUE);
                    gc_mystery_setup_screen(&run);
                    bool match = true;
                    for (int k = 0; k < n_tf && k < n_mons; k++) match = match && gc_weird_species(&run, k) == tf_species[k];
                    if (getenv("WD_DEBUG")) { fprintf(stderr, "steps (%d,%d):", ha + a, sa + b); for (int k = 0; k < n_tf && k < n_mons; k++) fprintf(stderr, " %d/%d", gc_weird_species(&run, k), tf_species[k]); fputc('\n', stderr); }
                    if (match) { found = true; searched++; }
                }
            }
            if (!found) { gc_weird_set_ranges(ha, sa); gc_mystery_record(&run, GC_ME_WEIRD_DREAM, GC_ME_ROGUE); gc_mystery_setup_screen(&run); }
        }
        CHECK(run.screen.n_options == 3, "%d options", run.screen.n_options);
        CHECK(same_rng(&run.run.rng, &after_init), "the screen moved the run RNG (c %.0f, upstream %.0f)", run.run.rng.c, after_init.c);

        /* onInit: the species and the Pokemon made */
        EQ("transformations", n_tf, n_mons);
        for (int k = 0; k < n_tf && k < n_mons; k++) {
            char nm[40];
            snprintf(nm, sizeof nm, "transformation %d previous position", k);
            EQ(nm, tf_prev[k], k);
            snprintf(nm, sizeof nm, "transformation %d species", k);
            EQ(nm, gc_weird_species(&run, k), tf_species[k]);
            snprintf(nm, sizeof nm, "transformation %d held items", k);
            int held = 0;
            for (int i = 0; i < run.n_items; i++) if (run.items[i].pokemon_id == run.party[run.party_order[k]].b.id && !gc_item_key_form_change(run.items[i].key)) held++;
            EQ(nm, held, tf_held[k]);
            gc_mon fresh;
            if (gc_weird_new_pokemon(&run, k, &fresh)) {
                snprintf(nm, sizeof nm, "new Pokemon %d", k);
                compare_mon(nm, &fresh, &tf[k]);
            } else CHECK(false, "new Pokemon %d could not be made", k);
            new_species_seen += !seen_species[tf_species[k] & 8191];
            seen_species[tf_species[k] & 8191] = 1;
            if (gc_form(tf_species[k], 0)->base_total > 600) over600++;
            transformations++;
        }
        CHECK(same_rng(&run.run.rng, &after_init), "the plan moved the run RNG");

        /* the option */
        bool ok = false;
        const char *w2 = "";
        gc_weird_hold_start(option == 1);   /* the first turn of the fight would switch the lead in and use up held items: compared after */
        gc_mystery_execute(&run, option, 0, 0, &w2, &ok);
        CHECK(ok, "option refused: %s", w2);
        CHECK(same_rng(&run.run.rng, &after_option), "the run RNG after the option differs (c %.0f, upstream %.0f)", run.run.rng.c, after_option.c);
        if (option == 0) {
            EQ("setEncounterRewards once", n_rewards, 1);
            EQ("leaveEncounterWithoutBattle once", leave_calls, 1);
            EQ("leaveEncounterWithoutBattle(false)", leave_arg, 0);
            EQ("the shop does not fill", rw_fill, 0);
            EQ("six reward functions", n_funcs, 6);
            const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
            EQ("six reward functions (C)", c->n_funcs, 6);
            for (int k = 0; k < c->n_funcs && k < n_funcs; k++) {
                char nm[40];
                snprintf(nm, sizeof nm, "reward function %d", k);
                EQ(nm, c->funcs[k], gc_reward_item_index(funcs[k]));
            }
            EQ("shop does not fill (C)", c->fill_remaining, 0);
            EQ("PartyHealPhase queued upstream", n_queued, 1);
            CHECK(n_queued == 1 && !strcmp(queued[0], "PartyHealPhase(true)"), "queued %s", queued[0]);
            EQ("PartyHealPhase queued", gc_rq_has(&run, RP_PARTY_HEAL), 1);
            EQ("a shop follows", gc_rq_has(&run, RP_SELECT_MODIFIER), 1);
            /* the dialogue: the cutscene, the dream's end, then a message for each new starter */
            int starters = 0;
            for (int k = 0; k < n_dialogue; k++) starters += !strcmp(dialogue[k], "addedAsAStarter");
            EQ("dialogue length", n_dialogue, 2 + starters);
            CHECK(n_dialogue >= 2 && !strcmp(dialogue[0], "option.1.cutscene") && !strcmp(dialogue[1], "option.1.dreamComplete"), "dialogue %s %s", dialogue[0], dialogue[1]);
            int said = 0, order_ok = 1, seen_end = 0;
            gc_event ev;
            while (gc_run_next_event(&run, &ev)) {
                if (ev.kind != GC_EV_MESSAGE || !ev.msg.key) continue;
                if (!strcmp(ev.msg.key, "mysteryEncounters/weirdDream:option.1.cutscene")) { order_ok &= said == 0 && !seen_end; said++; }
                if (!strcmp(ev.msg.key, "mysteryEncounters/weirdDream:option.1.dreamComplete")) { order_ok &= said == 1; seen_end = 1; said++; }
                if (!strcmp(ev.msg.key, "battle:addedAsAStarter")) { order_ok &= seen_end; said++; }
            }
            EQ("the texts said", said, n_dialogue);
            EQ("the texts in order", order_ok, 1);
            starters_said += starters;
        } else if (option == 1) {
            EQ("setEncounterRewards once", n_rewards, 1);
            EQ("leaveEncounterWithoutBattle never", leave_calls, 0);
            EQ("setEncounterRewards once (fight)", fr_n, 1);
            EQ("the shop does not fill", fr_fill, 0);
            EQ("no reward functions", fr_funcs, 0);
            EQ("a callback before the rewards", fr_cb, 1);
            EQ("six reward tiers", fr_n_tiers, 6);
            EQ("nothing queued", n_queued, 0);
            EQ("the dialogue is the fight's line", n_dialogue, 1);
            CHECK(n_dialogue == 1 && !strcmp(dialogue[0], "option.2.selected2"), "dialogue %s", dialogue[0]);
            EQ("a trainer battle", fd_mode, 1);
            EQ("a single battle", fd_double, 0);
            EQ("trainer type", run.trainer_type, fd_trainer);
            EQ("trainer variant", run.trainer_variant, fd_variant);
            EQ("the trainer has no name", fd_name[0] == '-', 1);
            EQ("a trainer battle (C)", run.battle_type, BATTLE_TYPE_TRAINER);
            EQ("single (C)", run.double_battle, 0);
            EQ("enemy count", run.n_enemy, fx_n);
            EQ("data source count", ftg_n, fx_n);
            for (int k = 0; k < run.n_enemy && k < fx_n; k++) {
                char nm[40];
                const gc_mon *e = &run.enemy[k];
                snprintf(nm, sizeof nm, "enemy %d", k);
                compare_mon(nm, e, &fxd[k]);
                snprintf(nm, sizeof nm, "enemy %d boss", k); EQ(nm, e->b.is_boss, fx[k].boss);
                snprintf(nm, sizeof nm, "enemy %d boss segments", k); EQ(nm, e->b.boss_segments, fx[k].segments);
                snprintf(nm, sizeof nm, "enemy %d id", k); EQ(nm, (long long)e->b.id, fx[k].id);
                snprintf(nm, sizeof nm, "enemy %d trainer slot", k); EQ(nm, e->trainer_slot, fx[k].slot);
                snprintf(nm, sizeof nm, "enemy %d b trainer slot", k); EQ(nm, e->b.trainer_slot, fx[k].slot);
                snprintf(nm, sizeof nm, "enemy %d pokerus", k); EQ(nm, e->pokerus, fx[k].pokerus);
                snprintf(nm, sizeof nm, "enemy %d luck", k); EQ(nm, e->luck, fx[k].luck);
                snprintf(nm, sizeof nm, "enemy %d pokeball", k); EQ(nm, e->pokeball, fx3[k].pokeball);
                snprintf(nm, sizeof nm, "enemy %d tera", k); EQ(nm, e->b.tera_type, fx3[k].tera);
                snprintf(nm, sizeof nm, "enemy %d ability", k); EQ(nm, pr_get_ability(&e->b, false), fx3[k].ability);
            }
            EQ("enemy item count", run.n_enemy_items, fj_n);
            for (int k = 0; k < run.n_enemy_items && k < fj_n; k++) {
                const gc_item *it = &run.enemy_items[k];
                int holder = -2;
                for (int e = 0; e < run.n_enemy; e++) if (run.enemy[e].b.id == it->pokemon_id) holder = e;
                CHECK(!strcmp(it->key, fj[k].key) && it->arg == fj[k].arg && holder == fj[k].holder && it->stack == fj[k].stack && (it->non_transferable ? 0 : 1) == fj[k].transferable,
                      "enemy item %d is %s/%d holder %d x%d %s, upstream %s/%d holder %d x%d %s", k, it->key, it->arg, holder, it->stack, it->non_transferable ? "locked" : "free",
                      fj[k].key, fj[k].arg, fj[k].holder, fj[k].stack, fj[k].transferable ? "free" : "locked");
                items_checked++;
            }
            win = true;
        } else {
            EQ("no rewards", n_rewards, 0);
            EQ("leaveEncounterWithoutBattle once", leave_calls, 1);
            EQ("leaveEncounterWithoutBattle(true)", leave_arg, 1);
            EQ("nothing queued", n_queued, 0);
            EQ("no dialogue", n_dialogue, 0);
            EQ("a shop follows", gc_rq_has(&run, RP_SELECT_MODIFIER), 1);
        }
        (void)n_after_party;
        EQ("party size after", run.n_party, n_after_party);
        for (int k = 0; k < run.n_party && k < n_after_party; k++) {
            char nm[40];
            snprintf(nm, sizeof nm, "Pokemon %d after", k);
            compare_mon(nm, &run.party[run.party_order[k]], &after[k]);
        }
        check_items("after", after_items, n_after, old_ids, n_mons);
        items_checked += n_after;
        check_dex("after", dex_after, n_dex);
        dex_checked += n_dex;
        if (win) {
            /* the win: the six tiers and the shop, the passive callback in the rewards phase's seed offset */
            gc_weird_victory(&run);
            const pr_reward_custom *c = &gc_int(&run)->reward.me_custom;
            EQ("reward tiers (C)", c->n_tiers, fr_n_tiers);
            for (int k = 0; k < c->n_tiers && k < fr_n_tiers; k++) { char nm[40]; snprintf(nm, sizeof nm, "reward tier %d", k); EQ(nm, c->tiers[k], fr_tiers[k]); }
            EQ("reward functions (C)", c->n_funcs, 0);
            EQ("shop does not fill (C)", c->fill_remaining, 0);
            EQ("a shop follows", gc_rq_has(&run, RP_SELECT_MODIFIER), 1);
            EQ("a trainer victory follows", gc_rq_has(&run, RP_TRAINER_VICTORY), 1);
            gc_weird_reward(&run);
            CHECK(same_rng(&run.run.rng, &br_after), "the run RNG after the passive callback differs (c %.0f, upstream %.0f)", run.run.rng.c, br_after.c);
            (void)br_before;
            for (int k = 0; k < run.n_party && k < n_mons; k++) { char nm[40]; snprintf(nm, sizeof nm, "passive of Pokemon %d after the win", k); EQ(nm, run.party[run.party_order[k]].b.passive_flag, br_passives[k]); }
            gc_weird_begin_fight(&run);
            EQ("the first turn is queued", gc_rq_has(&run, RP_TURN_INIT), 1);
        }
        if (bad) mismatches++;
        cases++; by_option[option]++;
        (void)new_species_seen;
    }
    printf("Mystery encounter Weird Dream (%d range histories searched): %d cases run (transformation %d, fight %d, refusal %d), %d transformations (%d species, %d above 600 BST, %d new starters said), %d registry species checked, "
           "%d items and %d dex entries compared, %d fields. TOTAL mismatches %d\n",
           searched, cases, by_option[0], by_option[1], by_option[2], transformations, new_species_seen, over600, starters_said, listed, items_checked, dex_checked, fields_checked, mismatches);
    return mismatches ? 1 : 0;
}
