/* The Global Trade System (src/data/mystery-encounters/encounters/global-trade-system-encounter.ts at pinned upstream
 * 8555c08).
 *
 * onInit (executeWithSeedOffset(.., waveIndex)): for every party member, in party order, three Pokemon to trade it for.
 * A legendary, sub-legendary or mythical member only gets the three level-5 Pokemon of its generation's pool; any other
 * three species picked by generateTradeOption(alreadyUsed, bst) (a base stat total within 100 of the member's form's,
 * widened by 10 each way until 21 species fit; paradox species and legendaries out; the shuffle pops a species the party
 * and the options so far do not hold), and then, only after all three are picked, the three `new EnemyPokemon(species, level)`
 * (the Pokemon constructor, its moveset, the shiny roll). The options are never kept here: they are worked out again
 * from the run (the offset stream starts from the wave seed, so it is the same each time).
 *
 * Check Trade Offers (option 0) and Wonder Trade (option 1) need two Pokemon allowed in battle and let any member be
 * picked, fainted or not. Wonder Trade's Pokemon is rolled when the party screen answers, which is after the option's
 * executeWithSeedOffset: on the run's own RNG (getRandomEncounterPokemon, one shiny reroll of 512/65536 times the Shiny Charm's
 * 2^(1 + stacks) capped at 4096, one hidden ability reroll of 1/64, and a common egg move of its root species when it is
 * neither shiny nor hidden). The option phase (seed offset * 500): the trader's name, the traded Pokemon leaves the party
 * (updateModifiers prunes its items), the received one, built from the enemy as it is (Poke Ball randInt(4), unseeded: gc_stub_rand;
 * the traded Pokemon's passive flag), joins the end of the party, the traded Pokemon's held items but form change items and
 * species stat boosters follow it, the dex takes the species (addPokemonDataToDexAndValidateAchievements: IVs and
 * setPokemonCaught without a message; its achievements are not ported) and the encounter leaves with the heal-only shop.
 *
 * Trade an Item (option 2): any Pokemon holding something transferable. Its tier (withTierFromPool in the player pool; Lucky
 * Egg, Golden Egg and White Herb are not in it) plus one, a guaranteed item of that tier from a fresh pool (never a TM; no
 * luck upgrades) rolled on the offset stream, then the item is lost (updateModifiers), the trader's name comes from the run's
 * RNG (the code after the first await runs outside the offset) and the encounter leaves with a shop of that one item.
 *
 * Not ported: the achievements the dex update validates; a " & " trader name shows its whole entry (the RNG draw that
 * picks one half is made); the `selected` text of an option, which the app shows. */
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define GTS_PARTY 6
#define GTS_LOG(...) ((void)0)

static const uint16_t legendary_pools[10][3] = {
    {0, 0, 0},
    {SPECIES_ID_RATTATA, SPECIES_ID_PIDGEY, SPECIES_ID_WEEDLE},
    {SPECIES_ID_SENTRET, SPECIES_ID_HOOTHOOT, SPECIES_ID_LEDYBA},
    {SPECIES_ID_POOCHYENA, SPECIES_ID_ZIGZAGOON, SPECIES_ID_TAILLOW},
    {SPECIES_ID_BIDOOF, SPECIES_ID_STARLY, SPECIES_ID_KRICKETOT},
    {SPECIES_ID_PATRAT, SPECIES_ID_PURRLOIN, SPECIES_ID_PIDOVE},
    {SPECIES_ID_BUNNELBY, SPECIES_ID_LITLEO, SPECIES_ID_SCATTERBUG},
    {SPECIES_ID_PIKIPEK, SPECIES_ID_YUNGOOS, SPECIES_ID_ROCKRUFF},
    {SPECIES_ID_SKWOVET, SPECIES_ID_WOOLOO, SPECIES_ID_ROOKIDEE},
    {SPECIES_ID_LECHONK, SPECIES_ID_FIDOUGH, SPECIES_ID_TAROUNTULA},
};

/* Paradox Pokemon are not legendary or mythical upstream, so they are listed */
static const uint16_t excluded[] = {
    SPECIES_ID_GREAT_TUSK, SPECIES_ID_SCREAM_TAIL, SPECIES_ID_BRUTE_BONNET, SPECIES_ID_FLUTTER_MANE, SPECIES_ID_SLITHER_WING,
    SPECIES_ID_SANDY_SHOCKS, SPECIES_ID_ROARING_MOON, SPECIES_ID_WALKING_WAKE, SPECIES_ID_GOUGING_FIRE, SPECIES_ID_RAGING_BOLT,
    SPECIES_ID_IRON_TREADS, SPECIES_ID_IRON_BUNDLE, SPECIES_ID_IRON_HANDS, SPECIES_ID_IRON_JUGULIS, SPECIES_ID_IRON_MOTH,
    SPECIES_ID_IRON_THORNS, SPECIES_ID_IRON_VALIANT, SPECIES_ID_IRON_LEAVES, SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_CROWN,
};

static bool is_excluded(int id)
{
    for (size_t i = 0; i < sizeof excluded / sizeof excluded[0]; i++) if (excluded[i] == id) return true;
    return false;
}

static bool is_special(int id)
{
    const pr_species_gen *sg = gc_species_gen(id);
    return sg && (sg->legendary || sg->sub_legendary || sg->mythical);
}

/* Phaser.Math.RND.pick / randSeedItem of an array: no roll when it has one entry */
static int pick_index(pr_rng *r, int n)
{
    return n <= 1 ? 0 : pr_rng_seed_int(r, n, 0);
}

/* ---------------------------------------------------------------- generateTradeOption */
/* generateTradeOption(alreadyUsed, originalBst): `bst` < 0 = no BST range. Returns the species id (0 if every candidate is used:
 * upstream would loop forever) */
static int generate_trade_option(pr_rng *r, const uint16_t *used, int n_used, int bst)
{
    static uint16_t list[PR_SPECIES_COUNT];
    int min = 0, cap = 9999;
    if (bst > 0) { cap = bst + 100; min = bst - 100; }
    for (;;) {
        int n = 0;
        for (int i = 0; i < PR_SPECIES_COUNT; i++) {
            const int id = pr_species_table[i].id;
            if (is_special(id) || is_excluded(id)) continue;
            const pr_form *f = gc_form(id, 0);
            const int total = f ? f->base_total : 0;
            if (total >= min && total <= cap) list[n++] = (uint16_t)id;
        }
        if (n > 20) {
            for (int i = n - 1; i > 0; i--) {
                const int j = pr_rng_integer_in_range(r, 0, i);
                const uint16_t t = list[i];
                list[i] = list[j];
                list[j] = t;
            }
            while (n > 0) {
                const int id = list[--n];
                bool seen = false;
                for (int k = 0; k < n_used; k++) if (used[k] == id) { seen = true; break; }
                if (!seen) return id;
            }
            return 0;
        }
        min -= 10;
        cap += 10;
    }
}

/* ---------------------------------------------------------------- new EnemyPokemon(species, level, NONE, false) */
/* the constructor on `r` (the Pokemon constructor, generateAndPopulateMoveset, the shiny roll: finish draws from run->run.rng,
 * so the stream is swapped in while it runs) */
static bool new_enemy(gc_run *run, pr_rng *r, int species, int level, gc_mon *out)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = species;
    ec.level = level;
    pr_enemy_gen g;
    if (pr_run_enemy_pokemon_base_rng(r, &run->run, &ec, &g) < 0) return false;
    gc_mon_new_enemy(run, out, species, level, &g, 0);
    if (!gc_generate_moveset(run, r, out)) gc_port_enemy_moveset(run, out);
    const pr_rng saved = run->run.rng;
    run->run.rng = *r;
    const int rc = pr_run_enemy_pokemon_finish(&run->run, &ec, &g);
    *r = run->run.rng;
    run->run.rng = saved;
    if (rc < 0) return false;
    gc_refresh_enemy(run, out, species, level, &g, 0);
    pr_calculate_stats(&run->bt, &out->b);
    out->b.hp = out->b.stats[0];
    return true;
}

/* ---------------------------------------------------------------- onInit */
static gc_mon options_store[GTS_PARTY][3];

/* getPokemonTradeOptions(): into options_store, by storage index. False if a Pokemon could not be made */
static bool trade_options(gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    uint16_t used[GTS_PARTY * 4];
    int n_used = 0;
    for (int k = 0; k < run->n_party; k++) used[n_used++] = run->party[run->party_order[k]].b.species;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        const gc_mon *m = &run->party[idx];
        const pr_species *sp = gc_species(m->b.species);
        if (is_special(m->b.species)) {
            const int gen = sp && sp->generation >= 1 && sp->generation <= 9 ? sp->generation : 1;
            for (int j = 0; j < 3; j++)
                if (!new_enemy(run, &r, legendary_pools[gen][j], 5, &options_store[idx][j])) return false;
        } else {
            const pr_form *f = gc_form(m->b.species, m->b.form);
            const int bst = f ? f->base_total : 0;
            int species[3];
            for (int j = 0; j < 3; j++) {
                species[j] = generate_trade_option(&r, used, n_used, bst);
                if (!species[j]) return false;
                used[n_used++] = (uint16_t)species[j];
            }
            for (int j = 0; j < 3; j++)
                if (!new_enemy(run, &r, species[j], m->b.level, &options_store[idx][j])) return false;
        }
    }
    return true;
}

bool gc_gts_trade_options(gc_run *run, int party_idx, gc_mon out[3])
{
    if (party_idx < 0 || party_idx >= run->n_party || !trade_options(run)) return false;
    memcpy(out, options_store[party_idx], sizeof options_store[0]);
    return true;
}

/* ---------------------------------------------------------------- held items */
/* PokemonHeldItemModifier.isTransferable: false for vitamins, the evolution trackers, Shuckle Juice, Old Gateau, Macho Brace
 * and form change items (their classes set it), and for an item explicitly locked */
bool gc_gts_item_transferable(const gc_item *it)
{
    if (!it->pokemon_id || it->detached || it->non_transferable) return false;
    if (!strcmp(it->key, "BASE_STAT_BOOSTER") || !strcmp(it->key, "MYSTERY_ENCOUNTER_SHUCKLE_JUICE")
        || !strcmp(it->key, "MYSTERY_ENCOUNTER_OLD_GATEAU") || !strcmp(it->key, "MYSTERY_ENCOUNTER_MACHO_BRACE")
        || !strcmp(it->key, "FORM_CHANGE_ITEM") || !strcmp(it->key, "RARE_FORM_CHANGE_ITEM")
        || !strncmp(it->key, "EVOLUTION_TRACKER_", 18)) return false;
    return true;
}

/* the held items of the party member `idx` the item trade offers, as indexes into run->items in getHeldItems() order */
int gc_gts_held_items(const gc_run *run, int idx, int out[GC_MAX_ITEMS])
{
    if (idx < 0 || idx >= run->n_party) return 0;
    int n = 0;
    for (int i = 0; i < run->n_items; i++)
        if (run->items[i].pokemon_id == run->party[idx].b.id && gc_gts_item_transferable(&run->items[i])) out[n++] = i;
    return n;
}

/* ---------------------------------------------------------------- the trader's name */
static char name_buf[2][160];
static int name_ring;

/* generateRandomTraderName(): a generic trainer class, a gender list where the class has them, an entry of the list and a half
 * of it when it is a pair ("A & B"). The name is the entry's locale key; a pair shows both halves */
static const char *trader_name(pr_rng *r)
{
    char *out = name_buf[name_ring ^= 1];
    const gc_trader_class *c = &gc_trader_classes[pick_index(r, gc_trader_class_count)];
    int list = 0;
    const char *gender = "";
    if (c->gendered) {
        list = pick_index(r, 2) == 0 ? 1 : 2;
        gender = list == 1 ? ".male" : ".female";
    }
    const int entry = pick_index(r, c->list[list].n);
    const int parts = c->list[list].parts[entry] - '0';
    (void)pick_index(r, parts);
    snprintf(out, sizeof name_buf[0], "$t(trainersCommon:%s%s.%d)", c->key, gender, entry + 1);
    return out;
}

static pr_msg_arg name_arg(const char *name, const char *text)
{
    pr_msg_arg a = {.name = name, .kind = PR_ARG_TEMPLATE, .s = text};
    return a;
}

/* ---------------------------------------------------------------- Wonder Trade */
/* Pokemon.generateShinyVariant() on the wave seed at the id, as run-core's finish makes it */
#define SHINY_VARIANT_CHANCE 4
#define SHINY_EPIC_CHANCE 1
static int shiny_variant(gc_run *run, const gc_mon *m)
{
    const pr_species_gen *sg = gc_species_gen(m->b.species);
    if (!sg) return 0;
    const pr_form_gen *fg = &pr_form_gens[sg->form_start + (sg->n_forms ? m->b.form : 0)];
    if (!fg->has_variants) return 0;
    pr_rng r = pr_run_seed_offset_rng(&run->run, (int)(m->b.id & 0xffffu), &run->run.wave_seed);
    const int rand = pr_rng_seed_int(&r, 10, 0);
    return rand >= SHINY_VARIANT_CHANCE ? 0 : rand >= SHINY_EPIC_CHANCE ? 1 : 2;
}

/* getRandomEncounterPokemon({level, speciesFunction: generateTradeOption(party), eventChance: 100 (no timed events here),
 * shinyRerolls: 1, hiddenRerolls: 1, hiddenAbilityChance: 64, shinyChance: 512, maxShinyChance: 4096}) and the common egg
 * move, on the run's RNG */
static bool wonder_trade_pokemon(gc_run *run, int level, gc_mon *out)
{
    pr_rng *r = &run->run.rng;
    uint16_t used[GTS_PARTY];
    for (int k = 0; k < run->n_party; k++) used[k] = run->party[run->party_order[k]].b.species;
    const int species = generate_trade_option(r, used, run->n_party, -1);
    if (!species || !new_enemy(run, r, species, level, out)) return false;
    pr_pokemon *p = &out->b;
    /* trySetShinySeed(512, true, 4096) */
    if (!p->shiny) {
        double threshold = 512;
        const int charms = gc_item_stack(run, "SHINY_CHARM");
        if (charms > 0) threshold *= pow(2, 1 + charms);
        if (threshold > 4096) threshold = 4096;
        p->shiny = pr_rng_seed_int(r, 65536, 0) < threshold;
    }
    if (p->shiny) {
        const int v = shiny_variant(run, out);
        if (v > out->variant) out->variant = (uint8_t)v;
        out->luck = (int8_t)(out->variant + 1);
    }
    /* tryRerollHiddenAbilitySeed(64) while the ability is not the hidden one */
    const pr_species_gen *sg = gc_species_gen(species);
    const bool has_hidden = sg && sg->has_hidden;
    if (out->ability_index != 2 && has_hidden) {
        double chance = 64;
        const int stacks = gc_item_stack(run, "ABILITY_CHARM");
        if (stacks > 0) chance *= pow(2, -1 - stacks);
        if (!pr_rng_seed_int(r, (int)chance, 0)) {
            out->ability_index = 2;
            p->base_ability = p->ability = (uint16_t)gc_form_ability(species, gc_form(species, p->form), 2);
        }
    }
    /* a common egg move when it is still not shiny and has no hidden ability */
    if (!p->shiny && (!has_hidden || out->ability_index < 2)) {
        const int root = gc_root_species(species);
        const gc_starter_egg_moves *e = NULL;
        for (int i = 0; i < gc_starter_egg_moves_count; i++)
            if (gc_starter_egg_moves_data[i].species == root) { e = &gc_starter_egg_moves_data[i]; break; }
        if (e) {
            const int move = e->egg_moves[pr_rng_seed_int(r, 3, 0)];
            bool has = false;
            for (int k = 0; k < p->n_moves; k++) if (p->moves[k] == move) has = true;
            if (!has) {
                if (p->n_moves < 4) {
                    gc_mon_set_move(out, p->n_moves, move);
                } else {
                    const int slot = pr_rng_seed_int(r, 4, 0);
                    gc_mon_set_move(out, slot, move);
                    p->pp_used[slot] = 0;
                    p->pp_up[slot] = 0;
                    p->max_pp_override[slot] = -1;
                }
            }
        }
    }
    return true;
}

/* ---------------------------------------------------------------- the trade */
/* removePokemonFromPlayerParty (updateModifiers prunes the traded Pokemon's items), the received Pokemon added at the end of the
 * party with the held items that travel, the dex, and the three messages */
static bool do_trade(gc_run *run, int traded_idx, const gc_mon *received, pr_rng *name_rng)
{
    const char *trader = trader_name(name_rng);
    const gc_mon *traded_mon = &run->party[traded_idx];
    const uint32_t traded_id = traded_mon->b.id;
    const bool traded_passive = traded_mon->b.passive_flag;
    static gc_item carried[GC_MAX_ITEMS];   /* static: the 3DS stack is 32 KB */
    int n_carried = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id != traded_id || it->detached) continue;
        if (!strcmp(it->key, "FORM_CHANGE_ITEM") || !strcmp(it->key, "RARE_FORM_CHANGE_ITEM")
            || !strcmp(it->key, "SPECIES_STAT_BOOSTER") || !strcmp(it->key, "RARE_SPECIES_STAT_BOOSTER")) continue;
        carried[n_carried++] = *it;
    }
    const pr_msg_arg traded_name = pr_arg_pokemon_name(&run->bt, "tradedPokemon", &traded_mon->b);
    const pr_msg_arg received_name = pr_arg_pokemon_name(&run->bt, "received", &received->b);

    gc_party_remove(run, traded_idx);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);

    /* addPlayerPokemon(.., dataSource): the enemy as it is, a PlayerPokemon; the passive flag of the traded one and a Poke Ball
     * of the first four, unseeded upstream */
    static gc_mon nm;
    nm = *received;
    nm.b.is_player = true;
    nm.b.on_field = false;
    nm.b.switch_out_status = true;
    nm.b.is_boss = false;
    nm.b.boss_segments = nm.b.boss_segment_index = 0;
    nm.b.passive_flag = traded_passive;
    nm.pokeball = (uint8_t)gc_stub_rand(run, 4);
    pr_reset_summon_data(&nm.b);
    pr_reset_turn_data(&nm.b);
    pr_update_passive(&run->bt, &nm.b);
    const int slot = run->n_party;
    run->party[slot] = nm;
    run->party_order[slot] = (int8_t)slot;
    run->n_party++;
    for (int k = 0; k < n_carried && run->n_items < GC_MAX_ITEMS; k++) {
        gc_item it = carried[k];
        it.pokemon_id = nm.b.id;
        run->items[run->n_items++] = it;
    }
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
    gc_items_to_battle(run);

    gc_ev_msg(run, "mysteryEncounters/globalTradeSystem:pokemonTradeSelected", 2, traded_name, name_arg("tradeTrainerName", trader));
    gc_ev_msg(run, "mysteryEncounters/globalTradeSystem:pokemonTradeGoodbye", 1, traded_name);
    gc_ev_msg(run, "mysteryEncounters/globalTradeSystem:tradeReceived", 2, name_arg("tradeTrainerName", trader), received_name);

    /* addPokemonDataToDexAndValidateAchievements */
    if (run->sys) {
        gc_system_update_species_dex_ivs(run->sys, gc_species_root_for_starter(nm.b.species), nm.b.ivs);
        gc_system_set_pokemon_caught_ex(run->sys, &run->party[slot], true, false, false, run->run.mode == GAME_MODES_DAILY, NULL);
    }
    gc_me_leave_with_heal_shop(run);
    return true;
}

/* ---------------------------------------------------------------- Trade an Item */
static bool item_trade(gc_run *run, int holder, int which, const char **why)
{
    int held[GC_MAX_ITEMS];
    const int n = gc_gts_held_items(run, holder, held);
    if (which < 0 || which >= n) { *why = "mysteryEncounters/globalTradeSystem:option.3.invalidSelection"; return false; }
    const gc_item chosen = run->items[held[which]];
    const int item = gc_reward_item_index(chosen.key);
    if (item < 0) { *why = "unknown item"; return false; }

    /* withTierFromPool(PLAYER, party): the first tier the item weighs in, else the cached one; the three items the pool lacks */
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    int tier = pr_reward_pregen_option(ctx, item, chosen.arg).tier;
    if (tier < 0 && chosen.type_tier > 0) tier = chosen.type_tier - 1;
    if (tier < 0) tier = 1;   /* ModifierTier.GREAT */
    if (!strcmp(chosen.key, "WHITE_HERB")) tier = 1;
    else if (!strcmp(chosen.key, "LUCKY_EGG")) tier = 2;
    else if (!strcmp(chosen.key, "GOLDEN_EGG")) tier = 3;
    if (tier < 4) tier++;   /* < ModifierTier.MASTER */

    pr_reward_custom custom;
    memset(&custom, 0, sizeof custom);
    custom.tiers[0] = (int8_t)tier;
    custom.n_tiers = 1;
    custom.allow_luck_upgrades = false;
    custom.reroll_multiplier = NAN;
    pr_reward_thresholds th;
    pr_reward_option got;
    const pr_rng saved = run->run.rng;
    run->run.rng = gc_me_option_rng(run);
    pr_reward_regenerate_thresholds(ctx, 0, &th);
    bool made = false;
    for (int tries = 0; tries < 10000 && !made; tries++) {
        if (pr_reward_get_player_options(ctx, &th, 1, NULL, 0, &custom, &got) != 1) break;
        made = strstr(pr_item_keys[got.type.item], "TM_") == NULL;
    }
    run->run.rng = saved;
    if (!made) { *why = "no item could be made"; return false; }

    pr_reward_custom *cs = &gc_int(run)->reward.me_custom;
    memset(cs, 0, sizeof *cs);
    cs->options[cs->n_options++] = got;
    cs->fill_remaining = false;
    cs->reroll_multiplier = NAN;
    cs->allow_luck_upgrades = true;

    /* loseHeldItem(modifier, false), then updateModifiers(true) */
    const pr_msg_arg chosen_name = gc_m_item("chosenItem", chosen.key);
    if (--run->items[held[which]].stack <= 0) gc_item_remove_at(run, held[which]);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);

    /* the name comes after the await: the run's own RNG */
    const char *trader = trader_name(&run->run.rng);
    gc_ev_msg(run, "mysteryEncounters/globalTradeSystem:itemTradeSelected", 3, chosen_name, name_arg("tradeTrainerName", trader),
              gc_m_item("itemName", pr_item_keys[got.type.item]));
    gc_me_end_shop(run);
    return true;
}

/* ---------------------------------------------------------------- the options */
bool gc_gts_pick_ok(const gc_run *run, int option, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    if (option == 2) {
        int held[GC_MAX_ITEMS];
        return gc_gts_held_items(run, idx, held) > 0;
    }
    return option == 0 || option == 1;
}

int gc_gts_allowed(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_party; i++) if (gc_mon_allowed(&run->party[i])) n++;
    return n;
}

/* option 0: b = the party member (storage index), c = which of its three offers; option 1: b = the member; option 2: b = the holder,
 * c = the item among gc_gts_held_items; option 3: leave */
bool gc_gts_execute(gc_run *run, int option, int b, int c, const char **why)
{
    if (option == 3) {
        gc_me_leave_with_heal_shop(run);
        return true;
    }
    if (b < 0 || b >= run->n_party) { *why = "no such Pokemon"; return false; }
    if (option == 2) return item_trade(run, b, c, why);
    if (option != 0 && option != 1) { *why = "no such option"; return false; }
    if (gc_gts_allowed(run) < 2) { *why = "needs two Pokemon allowed in battle"; return false; }
    if (option == 0) {
        if (c < 0 || c > 2) { *why = "no such offer"; return false; }
        if (!trade_options(run)) { *why = "trade offers could not be made"; return false; }
        const gc_mon *recv = &options_store[b][c];
        pr_rng name_rng = gc_me_option_rng(run);
        return do_trade(run, b, recv, &name_rng);
    }
    static gc_mon wonder;
    if (!wonder_trade_pokemon(run, run->party[b].b.level, &wonder)) { *why = "Wonder Trade could not be made"; return false; }
    pr_rng name_rng = gc_me_option_rng(run);
    return do_trade(run, b, &wonder, &name_rng);
}
