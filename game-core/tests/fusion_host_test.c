/* DNA Splicers through the reward screen, built by hand: the offer (a shop item or a free reward) with the two party picks, what a refusal
 * leaves alone, the party left without a hole, the learn-move phases queued behind the screen, the held items (Mystery Encounter stat
 * items and a full stack included), the status coin, and a save/load with the fusion halves. What upstream's PlayerPokemon.fuse() makes of
 * all of it is compared in fusion_diff_test (tools/fusiondiff.sh; the replay of tests/fixtures/fusion-splicers.native.txt.gz is in
 * `make test`), so the numbers here are the ones that don't depend on the stand-ins' data. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_run r, loaded;
static gc_system sys;
static unsigned char buf[GC_SAVE_MAX], mutated[GC_SAVE_MAX];
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr, "fusion host line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static const int SPECIES[4] = {SPECIES_ID_BULBASAUR, SPECIES_ID_CHARMANDER, SPECIES_ID_SQUIRTLE, SPECIES_ID_PIKACHU};
static const uint16_t MOVES[4][4] = {
    {MOVE_ID_TACKLE, MOVE_ID_GROWL, 0, 0}, {MOVE_ID_SCRATCH, MOVE_ID_EMBER, 0, 0},
    {MOVE_ID_WATER_GUN, MOVE_ID_TAIL_WHIP, 0, 0}, {MOVE_ID_THUNDERBOLT, MOVE_ID_LEER, 0, 0}};

/* n starters in storage order 0..n-1 (the first leads); the first one may be given four moves */
static void setup(int n, bool full_moveset)
{
    const char *why;
    gc_system_init_defaults(&sys);
    gc_run_new(&r, GAME_MODES_CLASSIC, "fusion-host", 777);
    gc_starter st[4];
    memset(st, 0, sizeof st);
    for (int i = 0; i < n; i++) {
        st[i].species = (uint16_t)SPECIES[i];
        st[i].tera_type = -1;
        memset(st[i].ivs, 15, 6);
        memcpy(st[i].moves, MOVES[i], sizeof st[i].moves);
    }
    if (full_moveset) { st[0].moves[2] = MOVE_ID_VINE_WHIP; st[0].moves[3] = MOVE_ID_LEER; }
    CHECK(gc_run_set_starters(&r, st, n, &why));
    r.sys = &sys;
    gc_event e;
    while (gc_run_next_event(&r, &e)) {}
    CHECK(r.n_party == n);
    for (int i = 0; i < n; i++) CHECK(r.party_order[i] == i && r.party[i].b.species == SPECIES[i]);
    r.money = 1000;
}
/* the reward screen with this one offer, the command that follows it queued behind (as the mint test does) */
static void screen(gc_offer o, bool shop)
{
    gc_rqueue *q = &gc_int(&r)->rq;
    gc_rq_reset(q);
    q->current = gc_rp(RP_SELECT_MODIFIER, 0, 0, 0);
    q->waiting = true;
    gc_rq_push(&r, gc_rp(RP_COMMAND, 0, 0, 0));
    memset(&r.screen, 0, sizeof r.screen);
    r.screen.kind = GC_SCREEN_REWARD;
    if (shop) { r.screen.n_shop = 1; r.screen.shop[0] = o; }
    else { r.screen.n_options = 1; r.screen.options[0] = o; }
}
static gc_offer splicers(int cost) { return (gc_offer){.key = "DNA_SPLICERS", .arg = 0, .cost = cost, .target = GC_TARGET_FUSE}; }
static bool buy(int primary, int donor, const char **why)
{
    gc_choice c = {GC_CH_SHOP, 0, (int16_t)primary, (int16_t)donor};
    return gc_run_choose(&r, &c, why);
}
static bool pick(int primary, int donor, const char **why)
{
    gc_choice c = {GC_CH_REWARD, 0, (int16_t)primary, (int16_t)donor};
    return gc_run_choose(&r, &c, why);
}
static int stack_of(uint32_t id, const char *key)
{
    int n = 0;
    for (int i = 0; i < r.n_items; i++) if (r.items[i].pokemon_id == id && !strcmp(r.items[i].key, key)) n += r.items[i].stack;
    return n;
}
static int held_by(uint32_t id)
{
    int n = 0;
    for (int i = 0; i < r.n_items; i++) if (r.items[i].pokemon_id == id) n++;
    return n;
}
static uint32_t xorshift(uint32_t x) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return x; }
static uint32_t hashname(const char *s) { uint32_t h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u; return h; }
static uint32_t crc32(const unsigned char *p, size_t n)
{
    uint32_t c = ~0u;
    for (size_t i = 0; i < n; i++) { c ^= p[i]; for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xedb88320u & (0u - (c & 1))); }
    return ~c;
}
static void refresh_crc(size_t n) { uint32_t crc = crc32(mutated + 20, n - 20); memcpy(mutated + 16, &crc, 4); }
static size_t section_at(const unsigned char *p, size_t n, const char *name)
{
    for (size_t at = 20; at + 8 <= n;) {
        uint32_t h, len;
        memcpy(&h, p + at, 4); memcpy(&len, p + at + 4, 4);
        if (h == hashname(name)) return at;
        at += 8 + len;
    }
    return 0;
}

/* the shop: a purchase takes the money, fuses, leaves the screen open, and queues each move of the member that left */
static void purchase(void)
{
    const char *why = NULL;
    CHECK(gc_reward_target("DNA_SPLICERS") == GC_TARGET_FUSE);
    setup(3, false);
    const gc_mon primary_before = r.party[0], donor_before = r.party[1], third_before = r.party[2];
    screen(splicers(300), true);
    CHECK(buy(0, 1, &why));
    CHECK(r.money == 700 && r.screen.kind == GC_SCREEN_REWARD);
    CHECK(r.n_party == 2);
    CHECK(r.party[0].b.id == primary_before.b.id && r.party[0].b.species == SPECIES_ID_BULBASAUR);
    CHECK(r.party[0].b.fusion_species == SPECIES_ID_CHARMANDER && r.party[0].b.fusion_gender_known);
    CHECK(r.party[0].b.fusion_ability_index == donor_before.b.ability_index);
    CHECK(r.party[1].b.id == third_before.b.id && r.party[1].b.species == SPECIES_ID_SQUIRTLE);   /* the storage closed over the gap */
    CHECK(!r.party[2].b.id && !r.party[2].b.species);
    CHECK(r.party_order[0] == 0 && r.party_order[1] == 1);
    CHECK(r.bt.n_party[0] == 2 && r.bt.party[0][0] == &r.party[0].b && r.bt.party[0][1] == &r.party[1].b && r.bt.field[0] == &r.party[0].b);
    CHECK(r.party[0].b.n_moves == primary_before.b.n_moves);   /* its own moves stay until the queue gets to the new ones */
    CHECK(!r.bt.n_unported && r.stubs[GC_STUB_REWARD_APPLY] == 0);
    /* LearnMovePhase for each move of the member that left, in its order, then the command that was waiting */
    gc_rphase ph;
    for (int i = 0; i < donor_before.b.n_moves; i++) {
        CHECK(gc_rq_next(&gc_int(&r)->rq, &ph) && ph.type == RP_LEARN_MOVE && ph.a == 0 && ph.b == donor_before.b.moves[i] && ph.c == 0);
    }
    CHECK(gc_rq_next(&gc_int(&r)->rq, &ph) && ph.type == RP_COMMAND);
    CHECK(!gc_rq_next(&gc_int(&r)->rq, &ph));
    /* the screen is still there: a second purchase of the same pair is not possible (the pair is fused now) */
    const gc_mon snapshot[2] = {r.party[0], r.party[1]};
    CHECK(!buy(0, 1, &why) && !strcmp(why, "partyUiHandler:anyEffect"));
    CHECK(r.money == 700 && r.n_party == 2 && !memcmp(snapshot, r.party, sizeof snapshot));
    /* and a member that is not fused takes the fused one? no: a fused member is refused whichever place it has */
    CHECK(!buy(1, 0, &why) && r.money == 700 && r.n_party == 2);
}

/* a pick that is not two different members is the reward still there: nothing is taken, nothing changes */
static void refusals(void)
{
    const char *why = NULL;
    static const int picks[][2] = {{0, 0}, {1, 1}, {-1, 1}, {0, -1}, {-1, -1}, {0, 3}, {3, 0}, {0, 4}, {7, 0}, {0, 30000}, {-30000, 1}};
    for (unsigned i = 0; i < sizeof picks / sizeof *picks; i++) {
        setup(3, false);
        screen(splicers(300), true);
        const gc_mon before[6];
        memcpy((void *)before, r.party, sizeof before);
        const int n_items = r.n_items;
        CHECK(!buy(picks[i][0], picks[i][1], &why));
        CHECK(why != NULL);
        CHECK(r.money == 1000 && r.n_party == 3 && r.n_items == n_items && !memcmp(before, r.party, sizeof before));
        CHECK(r.screen.kind == GC_SCREEN_REWARD);
        gc_rphase ph;
        CHECK(gc_rq_next(&gc_int(&r)->rq, &ph) && ph.type == RP_COMMAND);   /* no learn phase */
        CHECK(!gc_rq_next(&gc_int(&r)->rq, &ph));
        /* free: the same, and the screen stays */
        screen(splicers(-1), false);
        CHECK(!pick(picks[i][0], picks[i][1], &why));
        CHECK(r.n_party == 3 && !memcmp(before, r.party, sizeof before) && r.screen.kind == GC_SCREEN_REWARD);
    }
    /* a party of one has nobody to fuse */
    setup(1, false);
    screen(splicers(300), true);
    CHECK(!buy(0, 0, &why) && !buy(0, -1, &why) && r.n_party == 1 && r.money == 1000);
    /* a fused member is refused as the first and as the second pick, and money stays */
    setup(3, false);
    screen(splicers(300), true);
    CHECK(buy(0, 2, &why));
    CHECK(r.money == 700 && r.n_party == 2);
    CHECK(!buy(0, 1, &why) && !strcmp(why, "partyUiHandler:anyEffect"));
    CHECK(!buy(1, 0, &why) && !strcmp(why, "partyUiHandler:anyEffect"));
    CHECK(r.money == 700 && r.n_party == 2);
}

/* a free pick closes the screen, the queued phases teach what fits, and a full moveset asks */
static void learning(void)
{
    const char *why = NULL;
    setup(3, false);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.screen.kind == GC_SCREEN_COMMAND && r.n_party == 2);
    CHECK(r.party[0].b.n_moves == 4);
    CHECK(r.party[0].b.moves[0] == MOVE_ID_TACKLE && r.party[0].b.moves[1] == MOVE_ID_GROWL);
    CHECK(r.party[0].b.moves[2] == MOVE_ID_SCRATCH && r.party[0].b.moves[3] == MOVE_ID_EMBER);

    /* the donor is the lead: the member that stays moves to the first place, and learns there */
    setup(3, false);
    screen(splicers(-1), false);
    CHECK(pick(2, 0, &why));
    CHECK(r.n_party == 2 && r.party[0].b.species == SPECIES_ID_CHARMANDER && r.party[1].b.species == SPECIES_ID_SQUIRTLE);
    CHECK(r.party[1].b.fusion_species == SPECIES_ID_BULBASAUR && !r.party[0].b.fusion_species);
    CHECK(r.bt.field[0] == NULL);   /* the lead left; the field is filled again by the next battle's setup */
    CHECK(r.party[1].b.n_moves == 4 && r.party[1].b.moves[2] == MOVE_ID_TACKLE && r.party[1].b.moves[3] == MOVE_ID_GROWL);

    /* four moves known: each new one asks which to forget, and "none" skips it */
    setup(3, true);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.screen.kind == GC_SCREEN_LEARN_MOVE && r.screen.party == 0 && r.screen.move == MOVE_ID_SCRATCH);
    gc_choice forget = {GC_CH_FORGET, 1, 0, 0};
    CHECK(gc_run_choose(&r, &forget, &why));
    CHECK(r.party[0].b.moves[1] == MOVE_ID_SCRATCH);
    CHECK(r.screen.kind == GC_SCREEN_LEARN_MOVE && r.screen.party == 0 && r.screen.move == MOVE_ID_EMBER);
    gc_choice keep = {GC_CH_FORGET, -1, 0, 0};
    CHECK(gc_run_choose(&r, &keep, &why));
    CHECK(r.screen.kind == GC_SCREEN_COMMAND);
    CHECK(r.party[0].b.moves[0] == MOVE_ID_TACKLE && r.party[0].b.moves[1] == MOVE_ID_SCRATCH);
    CHECK(r.party[0].b.moves[2] == MOVE_ID_VINE_WHIP && r.party[0].b.moves[3] == MOVE_ID_LEER);
}

/* hp: the average of the two, the status: the one that has it, or the coin when both do */
static void hp_and_status(void)
{
    const char *why = NULL;
    setup(2, false);
    screen(splicers(-1), false);
    CHECK(r.party[0].b.hp == r.party[0].b.stats[STAT_HP] && r.party[1].b.hp == r.party[1].b.stats[STAT_HP]);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].b.hp == r.party[0].b.stats[STAT_HP] && r.party[0].b.status == STATUS_EFFECT_NONE);

    /* only the second one burns: the fusion burns */
    setup(2, false);
    r.party[1].b.status = STATUS_EFFECT_BURN;
    const uint32_t rng_before = gc_int(&r)->stub_rng;
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].b.status == STATUS_EFFECT_BURN && gc_int(&r)->stub_rng == rng_before);   /* nothing to flip a coin for */

    /* both have one: the coin picks (0 = the second one's), and it is drawn once */
    for (int coin = 0; coin < 2; coin++) {
        setup(2, false);
        r.party[0].b.status = STATUS_EFFECT_PARALYSIS;
        r.party[1].b.status = STATUS_EFFECT_POISON;
        uint32_t s = 1;
        while ((xorshift(s) % 2) != (uint32_t)coin) s++;
        gc_int(&r)->stub_rng = s;
        screen(splicers(-1), false);
        CHECK(pick(0, 1, &why));
        CHECK(r.party[0].b.status == (coin ? STATUS_EFFECT_PARALYSIS : STATUS_EFFECT_POISON));
        CHECK(gc_int(&r)->stub_rng == xorshift(s));
    }

    /* a fainted member fused with a living one comes back with a hit point at least, and the status of the living one */
    setup(2, false);
    r.party[0].b.hp = 0;
    r.party[0].b.status = STATUS_EFFECT_FAINT;
    r.party[1].b.status = STATUS_EFFECT_SLEEP;
    r.party[1].b.status_sleep_turns = 3;
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].b.hp >= 1 && r.party[0].b.status == STATUS_EFFECT_SLEEP && r.party[0].b.status_sleep_turns == 3);

    /* the average comes to no hit point at all (1 of 100 against none, in a body of about 20): a living second member still gives one */
    setup(2, false);
    r.party[0].b.hp = 0;
    r.party[0].b.status = STATUS_EFFECT_FAINT;
    r.party[1].b.stats[STAT_HP] = 100;
    r.party[1].b.hp = 1;
    r.party[1].b.status = STATUS_EFFECT_BURN;
    CHECK(r.party[0].b.stats[STAT_HP] < 50);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].b.hp == 1 && r.party[0].b.status == STATUS_EFFECT_BURN);

    /* two fainted ones: the fusion is fainted */
    setup(2, false);
    r.party[0].b.hp = r.party[1].b.hp = 0;
    r.party[0].b.status = r.party[1].b.status = STATUS_EFFECT_FAINT;
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].b.hp == 0);
}

/* what the second member held goes with the fusion, whether or not the party screen would let it move */
static void held_items(void)
{
    const char *why = NULL;
    setup(3, false);
    const uint32_t a = r.party[0].b.id, b = r.party[1].b.id, c = r.party[2].b.id;
    gc_item_add(&r, "LEFTOVERS", -1, a, 1);
    gc_item_add(&r, "LEFTOVERS", -1, b, 2);
    gc_item_add(&r, "BASE_STAT_BOOSTER", STAT_ATK, b, 3);          /* a vitamin: "not transferable" in Manage Items */
    gc_item_add(&r, "SHELL_BELL", -1, b, 1);
    gc_item_add(&r, "SHELL_BELL", -1, c, 1);                       /* another member's stays */
    gc_item_add(&r, "AMULET_COIN", -1, 0, 1);                      /* the party's own */
    const int with_bonus = r.party[0].b.stats[STAT_ATK];
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(stack_of(a, "LEFTOVERS") == 3 && stack_of(a, "BASE_STAT_BOOSTER") == 3 && stack_of(a, "SHELL_BELL") == 1);
    CHECK(held_by(b) == 0);
    CHECK(stack_of(c, "SHELL_BELL") == 1 && stack_of(0, "AMULET_COIN") == 1);
    CHECK(r.party[0].b.stats[STAT_ATK] > with_bonus);              /* the stats were worked out again with the vitamins on it */
    for (int i = 0; i < r.n_items; i++) CHECK(r.items[i].pokemon_id != b);

    /* a stack at its limit leaves what doesn't fit behind, and it goes with the second member */
    setup(2, false);
    const uint32_t x = r.party[0].b.id, y = r.party[1].b.id;
    gc_item_add(&r, "LEFTOVERS", -1, x, 4);
    gc_item_add(&r, "LEFTOVERS", -1, y, 3);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(stack_of(x, "LEFTOVERS") == 4 && held_by(y) == 0 && r.n_items == 1);

    /* Toxic Orb and Flame Orb are one kind of item: the other orb can't join the first one's stack */
    setup(2, false);
    const uint32_t p = r.party[0].b.id, q = r.party[1].b.id;
    gc_item_add(&r, "TOXIC_ORB", -1, p, 1);
    gc_item_add(&r, "FLAME_ORB", -1, q, 1);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(stack_of(p, "TOXIC_ORB") == 1 && stack_of(p, "FLAME_ORB") == 0 && held_by(q) == 0 && r.n_items == 1);
    /* and in Manage Items: the transfer is refused where the receiver holds the other orb */
    setup(2, false);
    gc_item_add(&r, "TOXIC_ORB", -1, r.party[0].b.id, 1);
    gc_item_add(&r, "FLAME_ORB", -1, r.party[1].b.id, 1);
    CHECK(!gc_item_transfer(&r, 1, 0, 1) && !gc_item_transfer(&r, 0, 1, 1));
    CHECK(r.n_items == 2 && r.items[0].pokemon_id == r.party[0].b.id && r.items[1].pokemon_id == r.party[1].b.id);
}

/* the fusion half's shine counts for the party's luck, and is part of the save */
static void luck_and_save(void)
{
    const char *why = NULL;
    setup(3, false);
    r.party[1].b.shiny = true;
    r.party[1].variant = 2;
    r.party[1].luck = 3;
    const int before = gc_party_luck(&r);
    screen(splicers(-1), false);
    CHECK(pick(0, 1, &why));
    CHECK(r.party[0].fusion_shiny && r.party[0].fusion_variant == 2 && r.party[0].fusion_luck == 3);
    CHECK(r.party[0].luck + r.party[0].fusion_luck == 3 && !r.party[0].b.shiny);
    CHECK(before == 3 && gc_party_luck(&r) == before);   /* the second member's shine now counts through the fusion half */

    CHECK(r.screen.kind == GC_SCREEN_COMMAND && gc_run_can_save(&r));
    const size_t n = gc_run_save(&r, buf, sizeof buf);
    CHECK(n > 0);
    CHECK(gc_run_load(&loaded, buf, n, &why));
    CHECK(loaded.load_dropped == 0 && loaded.n_party == 2);
    CHECK(loaded.party[0].b.fusion_species == SPECIES_ID_CHARMANDER);
    CHECK(loaded.party[0].fusion_shiny && loaded.party[0].fusion_variant == 2 && loaded.party[0].fusion_luck == 3);
    CHECK(!loaded.party[1].fusion_shiny && !loaded.party[1].fusion_variant && !loaded.party[1].fusion_luck);

    /* a save from before the section: it loads whole, the fusion half is the species alone, and nothing is counted as lost */
    memcpy(mutated, buf, n);
    const size_t at = section_at(mutated, n, "fusion");
    CHECK(at != 0);
    const uint32_t hidden = hashname("fusion-section-from-an-older-build");
    memcpy(mutated + at, &hidden, 4);
    refresh_crc(n);
    CHECK(gc_run_load(&loaded, mutated, n, &why));
    CHECK(loaded.load_dropped == 0 && loaded.party[0].b.fusion_species == SPECIES_ID_CHARMANDER);
    CHECK(!loaded.party[0].fusion_shiny && !loaded.party[0].fusion_variant && !loaded.party[0].fusion_luck);

    /* damaged halves are refused: a luck no shine makes, a variant that doesn't exist, a half on a Pokemon that is not fused */
    const size_t data = at + 8 + 2 + 3 * 6 + 2;   /* the header, the block's field list and its count */
    const struct { size_t offset; unsigned char value; } damage[] = {
        {data + 2, 9}, {data + 1, 7}, {data + 0, 2}, {data + 3 + 2, 1},   /* [1] has no fusion */
    };
    for (unsigned i = 0; i < sizeof damage / sizeof *damage; i++) {
        memcpy(mutated, buf, n);
        mutated[damage[i].offset] = damage[i].value;
        refresh_crc(n);
        CHECK(!gc_run_load(&loaded, mutated, n, &why));
        CHECK(why && strstr(why, "fusion half"));
    }
    /* a cut or foreign field list */
    memcpy(mutated, buf, n);
    const uint16_t wrong = 5;
    memcpy(mutated + at + 8, &wrong, 2);
    refresh_crc(n);
    CHECK(!gc_run_load(&loaded, mutated, n, &why));
    CHECK(why && strstr(why, "fusion halves"));
}

int main(void)
{
    purchase();
    refusals();
    learning();
    hp_and_status();
    held_items();
    luck_and_save();
    printf("fusion host: %u checks passed\n", checks);
    return 0;
}
