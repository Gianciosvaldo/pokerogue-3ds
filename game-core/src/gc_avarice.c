/* Absolute Avarice (src/data/mystery-encounters/encounters/absolute-avarice-encounter.ts at pinned upstream 8555c08).
 *
 * The encounter comes with six Berries or more in the party (a stack counts one); once it is on, every option is open. onInit (executeWithSeedOffset(..,
 * waveIndex)) rolls nothing: it takes note of the player's Berries - for every Pokemon of the party the list of its BerryModifiers, in the order of the
 * modifier list - and builds the Greedent's config: a boss of three bars, never shiny, a level boost of one level per ten waves, Thrash / Crunch / Body
 * Press / Slack Off, one Berry for every stack of every Berry the player has (a stack that does not fit on the boss is lost), and a post-summon effect (its
 * message, then Sp. Def, and from wave 50 Speed too, up one stage). onVisualsStart then takes ALL the player's Berries away. Nothing of the notes is kept
 * here: the Berries are still in the list until an option is chosen, and the notes are worked out again from it.
 *
 * Option 1, battle (the first await takes everything after it out of the option's seed offset: the run's own RNG): setEncounterRewards({fillRemaining:
 * true}, undefined, callback), the callback gives every Pokemon of the party that holds no Reviver Seed a Reviver Seed and says foodStash; Stuff Cheeks from
 * the Greedent at itself before the first command; initBattleWithEnemyConfig (the level on the run's RNG, then the boss, both IV rolls, the config over it,
 * the Berries as listed, no item rolls). The player cannot flee or catch it, and may switch.
 * Option 2, reason with it (the option's stream, inside the seed offset): each Pokemon of the party in party order gets back floor(n * 2 / 5) of its n stolen
 * Berries, one at a time, each drawn by Phaser's shuffle of what is left and a pop; a Pokemon already at the stack's maximum gets nothing. The encounter leaves
 * with the heal shop.
 * Option 3, let it have the food (the option's stream): a Greedent of the level of the party's highest minus two (fainted ones count; the first of equal
 * levels), made by the EnemyPokemon constructor with the shiny lock, with Thrash / Body Press / Stuff Cheeks / Slack Off and its passive on, is caught with
 * a Poke Ball (no text); the encounter leaves with the heal shop.
 *
 * Not ported: the Greedent's and the berries' sprites and tweens, the sounds. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define NS "mysteryEncounters/absoluteAvarice"
#define MAX_STOLEN 64
#define MAX_BOSS_ITEMS 16

/* BerryModifier.getMaxHeldItemCount */
static int berry_cap(int type)
{
    return type == BERRY_TYPE_SITRUS || type == BERRY_TYPE_LUM || type == BERRY_TYPE_LEPPA || type == BERRY_TYPE_ENIGMA ? 2 : 3;
}

static bool is_berry(const gc_item *it) { return !strcmp(it->key, "BERRY"); }

/* the Berries of the party member `id`, one entry per stack, in the order of the modifier list */
static int stolen_of(const gc_run *run, uint32_t id, int *types)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!is_berry(it) || it->pokemon_id != id) continue;
        for (int s = 0; s < it->stack && n < MAX_STOLEN; s++) types[n++] = it->arg;
    }
    return n;
}

/* bossModifierConfigs: one Berry per stack of every BerryModifier of the list, put on the boss one at a time (a Berry beyond the stack's maximum is lost) */
static int boss_items(const gc_run *run, gc_item *out)
{
    int n = 0;
    memset(out, 0, MAX_BOSS_ITEMS * sizeof *out);
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!is_berry(it)) continue;
        for (int s = 0; s < it->stack; s++) {
            int k = 0;
            while (k < n && out[k].arg != it->arg) k++;
            if (k == n) {
                if (n >= MAX_BOSS_ITEMS) continue;
                out[n++] = (gc_item){.key = "BERRY", .arg = it->arg, .stack = 1};
            } else if (out[k].stack < berry_cap(it->arg)) {
                out[k].stack++;
            }
        }
    }
    return n;
}

/* PersistentModifierRequirement("BerryModifier", 6): the stacks of BerryModifiers (not the Berry Pouch, which is a PreserveBerryModifier) */
bool gc_avarice_spawnable(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++) if (is_berry(&run->items[i])) n += run->items[i].stack;
    return n >= 6;
}

void gc_avarice_tokens(gc_run *run, pr_msg_arg *name)
{
    (void)run;
    *name = pr_arg_key("greedentName", "pokemon:greedent");
}

/* onVisualsStart: removeModifier for every BerryModifier, then updateModifiers */
static void steal_berries(gc_run *run)
{
    for (int i = 0; i < run->n_items; ) {
        if (is_berry(&run->items[i])) gc_item_remove_at(run, i);
        else i++;
    }
    gc_me_update_modifiers(run);
}

/* option 1 */
static void fight(gc_run *run, const gc_item *items, int n_items)
{
    gc_internal *in = gc_int(run);
    run->double_battle = false;
    /* the first await (transitionMysteryEncounterIntroVisuals) took the rest out of the option's seed offset: the run's RNG */
    pr_rng r = run->run.rng;
    const int base = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    const int level = base + (int)fmax(floor((double)run->wave / 10 * 1.0 + 0.5), 0);
    gc_mon *boss = &run->enemy[0];
    if (!gc_me_new_boss(run, &r, SPECIES_ID_GREEDENT, level, true, boss)) {
        /* a species or biome run-core has no rolls for: nobody guards the food, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->run.rng = r;
    run->n_enemy = 1;
    run->battle_type = BATTLE_TYPE_WILD;
    /* the config: three bars, never shiny, its moves */
    boss->b.boss_segments = 3;
    boss->b.boss_segment_index = 2;
    boss->b.is_boss = true;
    boss->b.passive_flag = false;
    boss->b.shiny = false;
    boss->b.n_moves = 0;
    memset(boss->b.moves, 0, sizeof boss->b.moves);
    memset(boss->b.pp_used, 0, sizeof boss->b.pp_used);
    memset(boss->b.pp_up, 0, sizeof boss->b.pp_up);
    for (int i = 0; i < 4; i++) boss->b.max_pp_override[i] = -1;
    gc_mon_set_move(boss, 0, MOVE_ID_THRASH);
    gc_mon_set_move(boss, 1, MOVE_ID_CRUNCH);
    gc_mon_set_move(boss, 2, MOVE_ID_BODY_PRESS);
    gc_mon_set_move(boss, 3, MOVE_ID_SLACK_OFF);
    boss->b.me_post_summon = run->wave < 50 ? PR_ME_POST_AVARICE_LOW : PR_ME_POST_AVARICE_HIGH;
    gc_enemy_finish_party_items(run, items, n_items);

    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = true;   /* the callback setEncounterRewards was given */
    in->me_reward.n_more = in->me_reward.n_tiers = 0;
    in->avarice.pending = true;
    in->me_no_flee = true;
    gc_me_add_start_effect(run, 2, 2, MOVE_ID_STUFF_CHEEKS);
    gc_me_begin_battle(run, GC_ME_ABSOLUTE_AVARICE);
}

/* givePartyPokemonReviverSeeds, from the rewards phase: a Reviver Seed for every Pokemon of the party that holds no instant-revive item, then foodStash */
void gc_avarice_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->avarice.pending) return;
    in->avarice.pending = false;
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        bool revives = false;
        for (int i = 0; i < run->n_items; i++) revives |= run->items[i].pokemon_id == m->b.id && !strcmp(run->items[i].key, "REVIVER_SEED");
        if (!revives) gc_item_add(run, "REVIVER_SEED", -1, m->b.id, 1);
    }
    gc_me_update_modifiers(run);
    gc_ev_msg(run, NS ":option.1.foodStash", 2, pr_arg_key("greedentName", "pokemon:greedent"),
              pr_arg_key("foodReward", "modifierType:ModifierType.REVIVER_SEED.name"));
    gc_items_to_battle(run);
}

/* option 2: the Berries each Pokemon gets back */
static void reason(gc_run *run, const int stolen[][MAX_STOLEN], const int *n_stolen)
{
    pr_rng r = gc_me_option_rng(run);
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        int list[MAX_STOLEN], n = n_stolen[k];
        memcpy(list, stolen[k], sizeof list);
        const int back = (int)floor((double)n * 2 / 5);
        for (int i = 0; i < back; i++) {
            /* Phaser.Math.RND.shuffle: for (i = len - 1; i > 0; i--) swap(array[i], array[floor(frac * (i + 1))]); then pop */
            for (int j = n - 1; j > 0; j--) {
                const int t = (int)floor(pr_rng_frac(&r) * (j + 1));
                const int v = list[j];
                list[j] = list[t];
                list[t] = v;
            }
            const int type = list[--n];
            /* applyModifierTypeToPlayerPokemon: a Pokemon already holding the maximum of that Berry gets nothing */
            int held = 0;
            for (int j = 0; j < run->n_items; j++) {
                const gc_item *it = &run->items[j];
                if (is_berry(it) && it->arg == type && it->pokemon_id == m->b.id) held += it->stack;
            }
            if (held < berry_cap(type)) gc_item_add(run, "BERRY", (int16_t)type, m->b.id, 1);
        }
    }
    gc_me_update_modifiers(run);
    gc_items_to_battle(run);
    gc_me_leave_with_heal_shop(run);
}

/* new EnemyPokemon(Greedent, level, NONE, false, true) on `r`: a wild enemy's generation and moveset, the shiny roll skipped by the lock */
static bool new_greedent(gc_run *run, pr_rng *r, int level, gc_mon *out)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = SPECIES_ID_GREEDENT;
    ec.level = level;
    ec.boss = false;
    ec.shiny_lock = true;
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.fusion_species = -1;
    g.friendship = -1;
    if (pr_run_enemy_pokemon_base_rng(r, &run->run, &ec, &g) < 0) return false;
    gc_mon_new_enemy(run, out, SPECIES_ID_GREEDENT, level, &g, 0);
    if (!gc_generate_moveset(run, r, out)) gc_port_enemy_moveset(run, out);
    const pr_rng saved = run->run.rng;
    run->run.rng = *r;
    const int rc = pr_run_enemy_pokemon_finish(&run->run, &ec, &g);
    *r = run->run.rng;
    run->run.rng = saved;
    if (rc < 0) return false;
    gc_refresh_enemy(run, out, SPECIES_ID_GREEDENT, level, &g, 0);
    pr_calculate_stats(&run->bt, &out->b);
    out->b.hp = out->b.stats[STAT_HP];
    return true;
}

/* option 3 */
static void let_it_have_it(gc_run *run)
{
    int high = 0;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (k == 0 || run->party[high].b.level < run->party[idx].b.level) high = idx;
    }
    int level = run->n_party > 0 ? run->party[high].b.level - 2 : 1;
    if (level < 1) level = 1;
    pr_rng r = gc_me_option_rng(run);
    gc_mon greedent;
    if (!new_greedent(run, &r, level, &greedent)) {
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return;
    }
    memset(greedent.b.moves, 0, sizeof greedent.b.moves);
    memset(greedent.b.pp_used, 0, sizeof greedent.b.pp_used);
    memset(greedent.b.pp_up, 0, sizeof greedent.b.pp_up);
    greedent.b.n_moves = 0;
    for (int i = 0; i < 4; i++) greedent.b.max_pp_override[i] = -1;
    gc_mon_set_move(&greedent, 0, MOVE_ID_THRASH);
    gc_mon_set_move(&greedent, 1, MOVE_ID_BODY_PRESS);
    gc_mon_set_move(&greedent, 2, MOVE_ID_STUFF_CHEEKS);
    gc_mon_set_move(&greedent, 3, MOVE_ID_SLACK_OFF);
    greedent.b.passive_flag = true;
    /* catchPokemon(greedent, null, POKEBALL, false): the Pokemon as an enemy, RP_CATCH (c = 2: no "obtained" text) */
    run->enemy[0] = greedent;
    run->n_enemy = 1;
    run->n_enemy_items = 0;
    gc_items_to_battle(run);
    gc_int(run)->thrown_ball = POKEBALL_TYPE_POKEBALL;
    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));
    gc_me_leave_with_heal_shop(run);
}

void gc_avarice_execute(gc_run *run, int option)
{
    static gc_item items[MAX_BOSS_ITEMS];
    static int stolen[PR_MAX_PARTY][MAX_STOLEN];
    int n_stolen[PR_MAX_PARTY];
    /* onInit's notes, made from the list as it is before the Berries are taken */
    const int n_items = boss_items(run, items);
    for (int k = 0; k < run->n_party && k < PR_MAX_PARTY; k++) n_stolen[k] = stolen_of(run, run->party[run->party_order[k]].b.id, stolen[k]);
    steal_berries(run);
    if (option == 0) fight(run, items, n_items);
    else if (option == 1) reason(run, (const int (*)[MAX_STOLEN])stolen, n_stolen);
    else let_it_have_it(run);
}
