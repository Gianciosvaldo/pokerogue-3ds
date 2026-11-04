/* Uncommon Breed (src/data/mystery-encounters/encounters/uncommon-breed-encounter.ts at pinned upstream 8555c08).
 *
 * onInit, in executeWithSeedOffset(.., waveIndex): the level is the party's highest level (fainted Pokemon count) minus 2; the Pokemon is
 * getRandomEncounterPokemon({level, isBoss: true}) (no timed event: no rerolls), then one of its four egg moves (randSeedInt(4); the root
 * species' list, unlocked or not) goes into its moveset: appended when it knows fewer than four moves, else in place of the first. The encounter
 * is for the common tier, waves 10..180, every biome; catching is allowed and fleeing is off.
 *
 * Option 1, battle (no await before initBattleWithEnemyConfig: the option's stream): the egg move starts the battle, used by the Pokemon with IGNORE_PP at
 * itself when it is a SelfStatusMove, else at the player's lead; setEncounterRewards({fillRemaining: true}); the Pokemon fights as a plain wild enemy
 * from its data source (isBoss false: no bars; the level and a new id on the option's stream, the items after the await on the run's RNG). Its
 * post-summon effect says its text and raises Def, SpDef and Spd (wave < 50) or Atk, Def, SpAtk, SpDef and Spd by one stage (battle-core).
 * Option 2, food (needs 4 BerryModifier stacks): four random berries leave the party (randSeedInt over the berry items, one stack each), then
 * the Pokemon learns a second egg move (randSeedInt(4) until it differs; appended when it knows fewer than four moves, else in place of the second)
 * and is caught, no ball thrown and no text (catchPokemon(.., POKEBALL, false)); the ordinary shop.
 * Option 3, charm (a Pokemon allowed in battle with a charming move; the primary Pokemon is the first): the same second egg move, every IV
 * rolled again (randSeedInt(31), the higher one stays), caught, setEncounterExp(primary, the Pokemon's exp value, no wave multiplier), the ordinary shop.
 *
 * Not ported: the sprite and its animation. A full party asks which member to release, as when a Pokemon is caught (RP_CATCH). */
#include "internal.h"
#include <math.h>
#include <string.h>

typedef struct {
    gc_mon mon;
    int level;
    int egg_move;   /* 0 = the species has no egg moves */
} breed_t;

/* getHighestLevelPlayerPokemon(false, true).level: the highest level of the party, fainted ones included */
static int highest_level(const gc_run *run)
{
    int hi = 0;
    for (int k = 0; k < run->n_party; k++) if (run->party[k].b.level > hi) hi = run->party[k].b.level;
    return hi;
}

/* PokemonMove pushed when the Pokemon knows fewer than four moves, else put in `replace` */
static void put_move(gc_mon *m, int move, int replace)
{
    if (m->b.n_moves < 4) gc_mon_set_move(m, m->b.n_moves, move);
    else gc_mon_set_move(m, replace, move);
}

/* onInit, on the stream of the wave seed; `after` is the stream where the rolls go on (none do); false when run-core has no roll for the biome
 * and wave or the species */
static bool breed_init(gc_run *run, breed_t *b)
{
    if (run->n_party < 1) return false;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    b->level = highest_level(run) - 2;
    if (!gc_me_roll_boss_at(run, &r, &b->mon, b->level)) return false;
    b->egg_move = 0;
    uint16_t eggs[4];
    const int n = gc_egg_moves(&b->mon, eggs);
    if (n > 0) {
        const int index = pr_rng_seed_int(&r, 4, 0);
        b->egg_move = index < n ? eggs[index] : 0;
        if (b->egg_move) put_move(&b->mon, b->egg_move, 0);
    }
    return true;
}

/* onInit's Pokemon as it stands after the egg move went in (the oracle's test reads it) */
bool gc_breed_roll(gc_run *run, gc_mon *mon, int *egg_move)
{
    static breed_t b;
    if (!breed_init(run, &b)) return false;
    *mon = b.mon;
    *egg_move = b.egg_move;
    return true;
}

bool gc_breed_preview(gc_run *run, int *species, int *form, bool *shiny, int *variant)
{
    static breed_t b;
    if (!breed_init(run, &b)) return false;
    *species = b.mon.b.species;
    *form = b.mon.b.form;
    *shiny = b.mon.b.shiny;
    *variant = b.mon.variant;
    return true;
}

/* the dialogue token onInit sets (enemyPokemon) for gc_mystery_setup_screen */
bool gc_breed_tokens(gc_run *run, pr_msg_arg *enemy_name)
{
    static breed_t b;
    if (!breed_init(run, &b)) return false;
    *enemy_name = pr_arg_pokemon_name(&run->bt, "enemyPokemon", &b.mon.b);
    return true;
}

/* PersistentModifierRequirement("BerryModifier", 4): the stacks of the held berries (the Berry items only) */
static int berry_stacks_held(const gc_run *run)
{
    int n = 0;
    for (int k = 0; k < run->n_items; k++) if (!strcmp(run->items[k].key, "BERRY")) n += run->items[k].stack;
    return n;
}

bool gc_breed_food_ok(const gc_run *run) { return berry_stacks_held(run) >= 4; }

/* givePokemonExtraEggMove: another egg move than the first (randSeedInt(4) until it differs), appended or put in the second place */
static void extra_egg_move(gc_mon *m, int previous, pr_rng *r)
{
    uint16_t eggs[4];
    const int n = gc_egg_moves(m, eggs);
    if (n <= 0) return;
    int move = 0;
    do {
        const int index = pr_rng_seed_int(r, 4, 0);
        move = index < n ? eggs[index] : 0;
    } while (move == previous);
    if (move) put_move(m, move, 1);
}

/* catchPokemon(pokemon, null, POKEBALL, false): the Pokemon as an enemy, RP_CATCH (c = 2: no "obtained" text) */
static void catch_it(gc_run *run, const gc_mon *mon)
{
    run->enemy[0] = *mon;
    run->n_enemy = 1;
    /* onInit's Pokemon holds nothing yet (the held items are rolled by initBattleWithEnemyConfig, which this option never reaches) */
    run->n_enemy_items = 0;
    gc_items_to_battle(run);
    gc_int(run)->thrown_ball = POKEBALL_TYPE_POKEBALL;
    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));
}

static void shop_and_leave(gc_run *run)
{
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

/* option 1 */
static void fight(gc_run *run, const breed_t *b)
{
    gc_internal *in = gc_int(run);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = false;
    if (b->egg_move) {
        /* the move's kind decides the target: itself for a SelfStatusMove, else the player's lead */
        const bool self = !strcmp(pr_moves[b->egg_move].kind, "SelfStatusMove");
        gc_me_add_start_effect(run, 2, self ? 2 : 0, b->egg_move);
    }
    gc_me_start_data_battle(run, &b->mon, b->level, run->wave < 50 ? PR_ME_POST_BREED_LOW : PR_ME_POST_BREED_HIGH, NULL, GC_ME_UNCOMMON_BREED);
}

/* option 2 */
static void food(gc_run *run, breed_t *b)
{
    pr_rng r = gc_me_option_rng(run);
    /* the held berries in item order: one stack each, an emptied item leaves the list */
    int idx[GC_MAX_ITEMS], n = 0;
    for (int k = 0; k < run->n_items; k++) if (!strcmp(run->items[k].key, "BERRY")) idx[n++] = k;
    for (int i = 0; i < 4 && n > 0; i++) {
        const int pick = pr_rng_seed_int(&r, n, 0);
        gc_item *it = &run->items[idx[pick]];
        it->stack--;
        if (it->stack == 0) {
            for (int j = pick; j < n - 1; j++) idx[j] = idx[j + 1];
            n--;
        }
    }
    /* the emptied items go, the rest keep their place */
    for (int k = run->n_items - 1; k >= 0; k--) if (!strcmp(run->items[k].key, "BERRY") && run->items[k].stack <= 0) gc_item_remove_at(run, k);
    gc_me_update_modifiers(run);
    /* after the await of updateModifiers: the run's own RNG */
    pr_rng after = run->run.rng;
    extra_egg_move(&b->mon, b->egg_move, &after);
    run->run.rng = after;
    catch_it(run, &b->mon);
    shop_and_leave(run);
}

/* option 3 */
static void charm(gc_run *run, breed_t *b)
{
    pr_rng r = gc_me_option_rng(run);
    extra_egg_move(&b->mon, b->egg_move, &r);
    for (int k = 0; k < 6; k++) {
        const int v = pr_rng_seed_int(&r, 31, 0);
        if (v > b->mon.b.ivs[k]) b->mon.b.ivs[k] = (uint8_t)v;
    }
    const int primary = gc_mystery_primary_pokemon(run, GC_ME_UNCOMMON_BREED, 2);
    catch_it(run, &b->mon);
    /* setEncounterExp(primary, pokemon.getExpValue(), false): base experience * level / 5 + 1 */
    if (primary >= 0) gc_me_exp_value(run, primary, gc_form(b->mon.b.species, b->mon.b.form)->base_exp * (double)b->mon.b.level / 5 + 1, false);
    shop_and_leave(run);
}

void gc_breed_execute(gc_run *run, int option)
{
    static breed_t b;
    if (!breed_init(run, &b)) {
        /* a species or biome run-core has no rolls for: nobody is there, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    if (option == 0) fight(run, &b);
    else if (option == 1) food(run, &b);
    else charm(run, &b);
}
