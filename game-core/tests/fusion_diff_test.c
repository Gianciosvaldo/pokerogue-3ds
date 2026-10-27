/* DNA Splicers against upstream at 8555c08: replays the trace of tests/fusion.producer.test.ts (a random party of 2-6 Pokemon of every
 * kind with held items in a shuffled order, one pair fused by FusePokemonModifierType's filter and FusePokemonModifier -> PlayerPokemon.fuse(),
 * all run for real) through game-core's gc_port_apply_reward -> gc_fusion_scene_apply, the party as game-core holds it (the case's
 * members at a rotated party_order, so a wrong party index shows).
 *
 * Per case, compared with what upstream did: the starting stats (the input check: game-core must compute the stats the trace
 * starts from, held vitamins and the Mystery Encounter stat items included); taken or refused (a pair with a fused member is
 * refused); then the party left, in order, member by member: the fusion half (species, form, gender, ability index, shiny, variant,
 * luck), the ability, HP, status and its counters, evolution pause, the six stats, the luck, the moveset; the whole item list entry by
 * entry (key, argument, holder, stack: what moved with the second member, what stayed behind when a stack was full, what was pruned);
 * the LearnMovePhases queued, in order, with the party place of the member that learns; whether the status coin was drawn; and that
 * the seeded RNG stayed where it was (the trace asserts that of upstream). Also game-core's own: the party storage compacted without
 * a hole, the battle's pointers into it, and that nothing was left unported.
 * Usage: fusion_diff_test <trace>; exit 0 only with 0 mismatches. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run run;
static FILE *f;

static long long integer(void) { long long v; if (fscanf(f, "%lld", &v) != 1) { fputs("malformed input\n", stderr); exit(2); } return v; }
static void word(char *out, size_t cap)
{
    char fmt[16];
    snprintf(fmt, sizeof fmt, "%%%zus", cap - 1);
    if (fscanf(f, fmt, out) != 1) { fputs("malformed input\n", stderr); exit(2); }
}
static void tag(char want)
{
    char c = 0;
    if (fscanf(f, " %c", &c) != 1 || c != want) { fprintf(stderr, "wrong record: wanted %c, got %c\n", want, c); exit(2); }
}
static void rest_of_line(void) { int ch; while ((ch = fgetc(f)) != EOF && ch != '\n') {} }
static void separator(void) { char s[4]; word(s, sizeof s); if (strcmp(s, ";")) { fputs("expected ;\n", stderr); exit(2); } }

typedef struct {
    long long id;
    int species, form, level, gender, nature, ability_index, shiny, variant, luck, friendship, ivs[6];
    int hp, status, toxic, sleep, freeze, pause, passive;
    int fusion, fusion_form, fusion_gender, fusion_ability, fusion_shiny, fusion_variant, fusion_luck;
    int stats[6], n_moves, moves[4];
} up_mon;
typedef struct { char key[48]; int arg, owner, stack; } up_item;
typedef struct {
    int orig, species, form, fusion, fusion_form, fusion_gender, fusion_ability, fusion_shiny, fusion_variant, fusion_luck;
    int ability_true, ability, hp, status, toxic, sleep, freeze, pause, stats[6], luck, get_luck, n_moves, moves[4];
} up_after;

static up_mon read_mon(void)
{
    up_mon u;
    u.id = integer(); u.species = (int)integer(); u.form = (int)integer(); u.level = (int)integer(); u.gender = (int)integer();
    u.nature = (int)integer(); u.ability_index = (int)integer(); u.shiny = (int)integer(); u.variant = (int)integer(); u.luck = (int)integer();
    u.friendship = (int)integer();
    for (int k = 0; k < 6; k++) u.ivs[k] = (int)integer();
    u.hp = (int)integer(); u.status = (int)integer(); u.toxic = (int)integer(); u.sleep = (int)integer(); u.freeze = (int)integer();
    u.pause = (int)integer(); u.passive = (int)integer();
    u.fusion = (int)integer(); u.fusion_form = (int)integer(); u.fusion_gender = (int)integer(); u.fusion_ability = (int)integer();
    u.fusion_shiny = (int)integer(); u.fusion_variant = (int)integer(); u.fusion_luck = (int)integer();
    for (int k = 0; k < 6; k++) u.stats[k] = (int)integer();
    u.n_moves = (int)integer();
    for (int k = 0; k < u.n_moves && k < 4; k++) u.moves[k] = (int)integer();
    return u;
}
static up_item read_item(void)
{
    up_item it;
    word(it.key, sizeof it.key);
    it.arg = (int)integer(); it.owner = (int)integer(); it.stack = (int)integer();
    return it;
}
static up_after read_after(void)
{
    up_after a;
    a.orig = (int)integer(); a.species = (int)integer(); a.form = (int)integer(); a.fusion = (int)integer(); a.fusion_form = (int)integer();
    a.fusion_gender = (int)integer(); a.fusion_ability = (int)integer(); a.fusion_shiny = (int)integer(); a.fusion_variant = (int)integer();
    a.fusion_luck = (int)integer(); a.ability_true = (int)integer(); a.ability = (int)integer(); a.hp = (int)integer(); a.status = (int)integer();
    a.toxic = (int)integer(); a.sleep = (int)integer(); a.freeze = (int)integer(); a.pause = (int)integer();
    for (int k = 0; k < 6; k++) a.stats[k] = (int)integer();
    a.luck = (int)integer(); a.get_luck = (int)integer(); a.n_moves = (int)integer();
    for (int k = 0; k < a.n_moves && k < 4; k++) a.moves[k] = (int)integer();
    return a;
}

/* gc_item.key is a pointer: every key the trace names lives here for good */
static const char *intern(const char *key)
{
    static char store[128][48];
    static int n;
    for (int i = 0; i < n; i++) if (!strcmp(store[i], key)) return store[i];
    if (n >= 128) { fputs("too many item keys\n", stderr); exit(2); }
    snprintf(store[n], sizeof store[n], "%.47s", key);
    return store[n++];
}

static uint32_t xorshift(uint32_t x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }

static int bad_inputs, mismatches;
#define MAX_MSG 8
static int shown;
#define CHECK(cond, ...) do { if (!(cond)) { bad = true; if (shown < 40) { shown++; fprintf(stderr, "case %d: ", case_no); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } } while (0)
#define EQ(what, got, want) CHECK((long long)(got) == (long long)(want), "%s %lld, upstream %lld", what, (long long)(got), (long long)(want))

/* the party: member i at storage party_order[i] */
static int storage_of_id(long long id)
{
    for (int s = 0; s < run.n_party; s++) if ((long long)run.party[s].b.id == id) return s;
    return -1;
}

static bool install(const up_mon *m, int n, const up_item *items, int n_items, int rot, int coin, uint32_t *stub_before, int case_no)
{
    memset(&run, 0, sizeof run);
    gc_run_new(&run, GAME_MODES_CLASSIC, "fusion-diff", 7);
    run.n_party = n;
    for (int i = 0; i < n; i++) run.party_order[i] = (int8_t)((i + rot) % n);
    for (int i = 0; i < n; i++) {
        gc_mon *gm = &run.party[run.party_order[i]];
        gc_starter st;
        memset(&st, 0, sizeof st);
        st.species = SPECIES_ID_MEW; st.tera_type = -1;
        memset(st.ivs, 15, 6);
        gc_mon_new_player(&run, gm, &st, m[i].level);
        pr_pokemon *p = &gm->b;
        p->id = (uint32_t)m[i].id;
        p->species = (uint16_t)m[i].species; p->form = (uint8_t)m[i].form;
        p->level = m[i].level; p->gender = (int8_t)m[i].gender; p->nature = (uint8_t)m[i].nature;
        for (int k = 0; k < 6; k++) p->ivs[k] = (uint8_t)m[i].ivs[k];
        gm->ability_index = (uint8_t)m[i].ability_index; p->ability_index = (int8_t)m[i].ability_index;
        p->shiny = m[i].shiny; gm->variant = (uint8_t)m[i].variant; gm->luck = (int8_t)m[i].luck; p->friendship = (uint8_t)m[i].friendship;
        p->passive_flag = m[i].passive != 0;
        p->passive = 0;
        p->custom_ability = p->fusion_custom_ability = p->custom_passive = -1;
        gm->pause_evolutions = m[i].pause != 0;
        p->fusion_species = 0; p->fusion_form = 0; p->fusion_gender = 0; p->fusion_gender_known = false; p->fusion_ability_index = -1;
        if (m[i].fusion >= 0) {
            p->fusion_species = (uint16_t)m[i].fusion; p->fusion_form = (uint8_t)m[i].fusion_form;
            p->fusion_gender = (int8_t)m[i].fusion_gender; p->fusion_gender_known = true;
            p->fusion_ability_index = (int8_t)m[i].fusion_ability;
            gm->fusion_shiny = m[i].fusion_shiny != 0; gm->fusion_variant = (uint8_t)m[i].fusion_variant; gm->fusion_luck = (int8_t)m[i].fusion_luck;
        }
        const pr_species *sp = gc_species(p->species);
        if (!sp || p->form >= sp->nforms || (p->fusion_species && !gc_species(p->fusion_species))) return false;
        if (p->fusion_species) {
            const pr_form *ff = gc_form(p->fusion_species, p->fusion_form);
            const int fi = p->fusion_ability_index;
            p->base_ability = fi == 0 ? ff->ability1 : fi == 1 ? ff->ability2 : ff->ability_hidden;
        } else {
            p->base_ability = (uint16_t)gc_form_ability(p->species, gc_form(p->species, p->form), m[i].ability_index);
        }
        p->ability = p->base_ability;
        pr_refresh_ability(p);
        p->n_moves = 0;
        for (int k = 0; k < m[i].n_moves && k < 4; k++) gc_mon_set_move(gm, k, m[i].moves[k]);
        p->n_moves = (uint8_t)m[i].n_moves;
    }
    for (int i = 0; i < n_items; i++) {
        const uint32_t owner = items[i].owner >= 0 ? (uint32_t)m[items[i].owner].id : 0;
        gc_item_add(&run, intern(items[i].key), (int16_t)items[i].arg, owner, items[i].stack);
    }
    run.bt.n_party[0] = (uint8_t)n;
    for (int k = 0; k < n; k++) run.bt.party[0][k] = &run.party[run.party_order[k]].b;
    run.bt.field[0] = run.bt.party[0][0];
    gc_items_to_battle(&run);
    /* the input check: the stats the trace starts from */
    for (int i = 0; i < n; i++) {
        gc_mon *gm = &run.party[run.party_order[i]];
        pr_pokemon *p = &gm->b;
        pr_update_passive(&run.bt, p);
        pr_calculate_stats(&run.bt, p);
        for (int k = 0; k < 6; k++)
            if (p->stats[k] != m[i].stats[k]) {
                if (bad_inputs < 12) fprintf(stderr, "case %d: INPUT member %d (species %d form %d) stat %d is %d here, upstream %d\n", case_no, i, p->species, p->form, k, p->stats[k], m[i].stats[k]);
                return false;
            }
        p->hp = m[i].hp;
        p->status = (uint8_t)m[i].status; p->status_toxic_turns = (int16_t)m[i].toxic;
        p->status_sleep_turns = (int16_t)m[i].sleep; p->status_freeze_turns = (int16_t)m[i].freeze;
    }
    /* the status coin: the stand-ins' generator, set so that its first draw is the case's */
    uint32_t s = 1;
    while ((xorshift(s) % 2) != (uint32_t)coin) s++;
    gc_int(&run)->stub_rng = s;
    *stub_before = s;
    return true;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fputs("usage: fusion_diff_test <trace>\n", stderr); return 2; }
    f = fopen(argv[1], "r");
    if (!f) { perror(argv[1]); return 2; }
    tag('V');
    if (integer() != 1) { fputs("unknown trace version\n", stderr); return 2; }
    integer();
    int cases = 0, fused = 0, refused = 0, coins = 0, no_coins = 0, items_checked = 0, learns = 0, fields = 0, skipped = 0;
    for (;;) {
        char c = 0;
        if (fscanf(f, " %c", &c) != 1 || c == '#') break;
        if (c != 'C') { fprintf(stderr, "wrong record: wanted C, got %c\n", c); return 2; }
        const int case_no = (int)integer();
        const int n = (int)integer(), primary = (int)integer(), donor = (int)integer(), coin = (int)integer();
        up_mon m[6];
        for (int i = 0; i < n; i++) { tag('M'); m[i] = read_mon(); }
        tag('I');
        const int n_items = (int)integer();
        static up_item items[128];
        for (int i = 0; i < n_items; i++) items[i] = read_item();
        tag('F');
        const int refused_up = (int)integer();
        tag('R');
        const int applied = (int)integer();
        int draws = 0, n_after = 0, n_items_after = 0, n_learn = 0;
        static up_after after[6];
        static up_item items_after[128];
        static int learn[8][2];
        if (applied) {
            draws = (int)integer();
            tag('S');
            n_after = (int)integer();
            for (int i = 0; i < n_after; i++) { if (i) separator(); after[i] = read_after(); }
            tag('J');
            n_items_after = (int)integer();
            for (int i = 0; i < n_items_after; i++) items_after[i] = read_item();
            tag('L');
            n_learn = (int)integer();
            for (int i = 0; i < n_learn && i < 8; i++) { learn[i][0] = (int)integer(); learn[i][1] = (int)integer(); }
        }
        tag('E');
        rest_of_line();
        cases++;

        bool bad = false;
        uint32_t stub_before = 0;
        if (n_items > 128 || !install(m, n, items, n_items, case_no % n, coin, &stub_before, case_no)) { bad_inputs++; skipped++; continue; }
        gc_offer offer;
        memset(&offer, 0, sizeof offer);
        offer.key = "DNA_SPLICERS"; offer.arg = 0; offer.target = GC_TARGET_FUSE; offer.tier = 1;
        const char *why = NULL;
        const int primary_storage = run.party_order[primary], donor_storage = run.party_order[donor];
        const long long lead_id = m[0].id;
        const bool ok = gc_port_apply_reward(&run, &offer, primary_storage, donor_storage, &why);
        EQ("taken", ok, applied);
        EQ("refused by the filter (upstream)", refused_up, !applied);
        if (!ok || !applied) {
            /* a refusal changes nothing */
            EQ("party size after a refusal", run.n_party, n);
            EQ("items after a refusal", run.n_items, n_items);
            EQ("stub rng after a refusal", gc_int(&run)->stub_rng, stub_before);
            gc_rphase ph;
            CHECK(!gc_rq_next(&gc_int(&run)->rq, &ph), "a refusal queued a phase");
            if (bad) mismatches++;
            if (applied == 0) refused++;
            continue;
        }
        fused++;
        EQ("party size", run.n_party, n_after);
        EQ("coin drawn", gc_int(&run)->stub_rng != stub_before, draws);
        if (draws) coins++; else no_coins++;
        if (run.n_party == n_after) {
            for (int k = 0; k < n_after; k++) {
                const up_after *a = &after[k];
                const int storage = run.party_order[k];
                CHECK(storage >= 0 && storage < run.n_party, "party order %d -> storage %d", k, storage);
                if (storage < 0 || storage >= run.n_party) continue;
                const gc_mon *gm = &run.party[storage];
                const pr_pokemon *p = &gm->b;
                EQ("member", (long long)p->id, m[a->orig].id);
                EQ("species", p->species, a->species); EQ("form", p->form, a->form);
                EQ("fusion species", p->fusion_species ? p->fusion_species : -1, a->fusion);
                if (a->fusion >= 0) {
                    EQ("fusion form", p->fusion_form, a->fusion_form); EQ("fusion gender", p->fusion_gender, a->fusion_gender);
                    EQ("fusion ability index", p->fusion_ability_index, a->fusion_ability);
                    EQ("fusion shiny", gm->fusion_shiny, a->fusion_shiny); EQ("fusion variant", gm->fusion_variant, a->fusion_variant);
                    EQ("fusion luck", gm->fusion_luck, a->fusion_luck);
                    CHECK(p->fusion_gender_known, "fusion gender not known");
                }
                EQ("base ability", p->base_ability, a->ability_true); EQ("ability", p->ability, a->ability);
                EQ("hp", p->hp, a->hp); EQ("status", p->status, a->status);
                EQ("toxic turns", p->status_toxic_turns, a->toxic); EQ("sleep turns", p->status_sleep_turns, a->sleep);
                EQ("freeze turns", p->status_freeze_turns, a->freeze);
                EQ("pause evolutions", gm->pause_evolutions, a->pause);
                for (int s = 0; s < 6; s++) EQ("stat", p->stats[s], a->stats[s]);
                EQ("luck", gm->luck, a->luck);
                EQ("getLuck", gm->luck + (p->fusion_species ? gm->fusion_luck : 0), a->get_luck);
                EQ("moves", p->n_moves, a->n_moves);
                for (int s = 0; s < a->n_moves && s < 4; s++) EQ("move", p->moves[s], a->moves[s]);
                fields++;
            }
        }
        /* the storage has no hole, and the battle's pointers follow it */
        EQ("battle party size", run.bt.n_party[0], run.n_party);
        for (int k = 0; k < run.n_party; k++) CHECK(run.bt.party[0][k] == &run.party[run.party_order[k]].b, "battle party pointer %d", k);
        for (int s = 0; s < 6; s++) {
            gc_mon zero;
            memset(&zero, 0, sizeof zero);
            if (s >= run.n_party) CHECK(!memcmp(&run.party[s], &zero, sizeof zero), "storage %d past the party is not clear", s);
        }
        {
            const int lead = storage_of_id(lead_id);
            const pr_pokemon *want = lead >= 0 ? &run.party[lead].b : NULL;
            CHECK(run.bt.field[0] == want, "the lead's pointer: %p, want %p (lead %s)", (void *)run.bt.field[0], (void *)want, lead >= 0 ? "kept" : "was the second member");
        }
        /* the item list */
        EQ("items", run.n_items, n_items_after);
        for (int i = 0; i < run.n_items && i < n_items_after; i++) {
            const gc_item *it = &run.items[i];
            const up_item *u = &items_after[i];
            int owner = -1;
            if (it->pokemon_id) for (int k = 0; k < n; k++) if ((long long)it->pokemon_id == m[k].id) owner = k;
            CHECK(!strcmp(it->key, u->key), "item %d is %s, upstream %s", i, it->key, u->key);
            EQ("item argument", it->arg, u->arg); EQ("item holder", owner, u->owner); EQ("item stack", it->stack, u->stack);
            items_checked++;
        }
        /* the LearnMovePhases */
        gc_rphase ph;
        for (int i = 0; i < n_learn && i < 8; i++) {
            const bool got = gc_rq_next(&gc_int(&run)->rq, &ph);
            CHECK(got, "learn phase %d missing", i);
            if (!got) break;
            EQ("phase type", ph.type, RP_LEARN_MOVE);
            int k = -1;
            for (int j = 0; j < run.n_party; j++) if (run.party_order[j] == ph.a) k = j;
            EQ("learning member's place", k, learn[i][0]); EQ("learned move", ph.b, learn[i][1]);
            EQ("learn move source", ph.c, 0);
            learns++;
        }
        CHECK(!gc_rq_next(&gc_int(&run)->rq, &ph), "an extra phase was queued (type %d)", ph.type);
        CHECK(!run.bt.n_unported, "something was left unported (%u)", (unsigned)run.bt.n_unported);
        if (bad) mismatches++;
    }
    printf("DNA Splicers: %d cases (%d fused, %d refused, %d not runnable), %d coin flips drawn (%d not needed), %d members compared, %d items compared, "
           "%d learn phases. TOTAL mismatches %d (input %d)\n",
           cases, fused, refused, skipped, coins, no_coins, fields, items_checked, learns, mismatches, bad_inputs);
    return mismatches || bad_inputs ? 1 : 0;
}
