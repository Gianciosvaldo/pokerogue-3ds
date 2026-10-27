/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#include "gc/system.h"
#include "gc/data.h"
#include "gc/run.h"
#include "internal.h"
#include <stdio.h>

int gc_system_starter_unlock_cost(uint16_t species) {
    static const uint8_t costs[10]={40,40,35,30,25,20,15,10,10,10};
    int cost=gc_starter_cost(species);
    return cost>=1 && cost<=10 ? costs[cost-1] : -1;
}

int gc_system_starter_reduction_cost(const gc_system *sys,uint16_t species){
    static const uint8_t costs[10][2]={{25,60},{25,60},{20,50},{15,40},{12,35},{10,30},{8,20},{5,15},{5,15},{5,15}};
    int base=gc_starter_cost(species);
    if(!sys||species>GC_MAX_SPECIES_ID||base<1||base>10||!sys->dex[species].caught_attr||sys->starters[species].value_reduction>=2)return -1;
    return costs[base-1][sys->starters[species].value_reduction];
}
bool gc_system_buy_starter_reduction(gc_system *sys,uint16_t species){
    int cost=gc_system_starter_reduction_cost(sys,species);
    if(cost<0||sys->starters[species].candy_count<cost)return false;
    sys->starters[species].candy_count-=cost;sys->starters[species].value_reduction++;return true;
}

bool gc_system_buy_starter_unlock(gc_system *sys, uint16_t species, bool hidden) {
    int cost=gc_system_starter_unlock_cost(species);
    if (!sys || species>GC_MAX_SPECIES_ID || cost<0 || !sys->dex[species].caught_attr) return false;
    const pr_species *sp=gc_species(species);
    if (!sp || !sp->nforms) return false;
    const pr_form *form=&pr_forms[sp->form0];
    gc_starter_entry *s=&sys->starters[species];
    if (s->candy_count<cost) return false;
    if (hidden) {
        if (!form->ability_hidden || (s->ability_attr&4)) return false;
        s->ability_attr|=4;
        s->ability_preference=2;s->preference_flags|=4;
    } else {
        if (!form->passive || (s->passive_attr&1)) return false;
        s->passive_attr|=3;
    }
    s->candy_count-=cost;
    return true;
}
#include <string.h>

void gc_system_init_defaults(gc_system *sys) {
    if (!sys) return;
    memset(sys, 0, sizeof(*sys));

    /* Upstream initDexData:
     * defaultStarterAttr = DexAttr.NON_SHINY | DexAttr.MALE | DexAttr.FEMALE | DexAttr.DEFAULT_VARIANT | DexAttr.DEFAULT_FORM
     * = 1 | 4 | 8 | 16 | 128 = 157 (0x9D) */
    const uint64_t default_starter_attr = 157ULL;

    for (int i = 0; i < gc_default_starter_count; i++) {
        uint16_t species = gc_default_starters[i];
        if (species > 0 && species <= GC_MAX_SPECIES_ID) {
            sys->dex[species].seen_attr = default_starter_attr;
            sys->dex[species].caught_attr = default_starter_attr;
            sys->dex[species].nature_attr = 1U << (gc_default_starter_natures[i] + 1);
            for (int k = 0; k < 6; k++) {
                sys->dex[species].ivs[k] = 15;
            }
        }
    }

    /* Upstream initStarterData:
     * abilityAttr = defaultStarterSpecies.includes(speciesId) ? AbilityAttr.ABILITY_1 : 0 */
    for (int i = 0; i < gc_starter_cost_count; i++) {
        uint16_t species = gc_starter_costs[i].id;
        if (species > 0 && species <= GC_MAX_SPECIES_ID) {
            /* If it's a default starter, caught_attr was set above */
            if (sys->dex[species].caught_attr != 0) {
                sys->starters[species].ability_attr = 1; /* ABILITY_1 */
            } else {
                sys->starters[species].ability_attr = 0;
            }
        }
    }

    sys->unlocks.endless_mode = false;
    sys->unlocks.mini_black_hole = false;
    sys->unlocks.spliced_endless_mode = false;
    sys->unlocks.eviolite = false;
}

uint64_t gc_mon_dex_attr(const gc_mon *m) {
    if (!m) return 0;
    uint64_t attr = 0;
    if (m->b.gender == 0) attr |= 4ULL;       /* MALE (0) */
    else if (m->b.gender == 1) attr |= 8ULL;  /* FEMALE (1) */
    attr |= m->b.shiny ? 2ULL : 1ULL;         /* SHINY / NON_SHINY */
    if (m->variant >= 2) attr |= 64ULL;       /* VARIANT_3 */
    else if (m->variant == 1) attr |= 32ULL;  /* VARIANT_2 */
    else attr |= 16ULL;                      /* DEFAULT_VARIANT */
    attr |= (1ULL << (7 + m->b.form));        /* DEFAULT_FORM = 128 (7 + 0) */
    return attr;
}

int16_t gc_species_prevolution(uint16_t species_id) {
    if (species_id == 0) return 0;
    for (int i = 0; i < gc_species_dex_info_count; i++) {
        if (gc_species_dex_info_data[i].species == species_id) {
            return gc_species_dex_info_data[i].prevolution;
        }
    }
    return 0;
}

uint16_t gc_species_root_id(uint16_t species_id) {
    uint16_t root = species_id;
    for (int depth = 0; depth < 10; depth++) {
        int16_t pre = gc_species_prevolution(root);
        if (pre <= 0) break;
        root = (uint16_t)pre;
    }
    return root;
}

void gc_system_unlock_species_nature(gc_system *sys, uint16_t species_id, uint8_t nature) {
    if (!sys || nature > 24 || !species_id || species_id > GC_MAX_SPECIES_ID
        || !gc_species(species_id)) return;
    const uint16_t root = gc_species_root_id(species_id);
    if (!sys->dex[root].caught_attr) return;
    for (int depth = 0; depth < 10; depth++) {
        sys->dex[species_id].nature_attr |= 1U << (nature + 1);
        const int16_t pre = gc_species_prevolution(species_id);
        if (pre <= 0) break;
        species_id = (uint16_t)pre;
    }
}

/* getRootSpeciesId(forStarter = true): down the prevolutions, stopping at the first species that is a starter
 * (Pikachu stops at Pikachu, a species whose chain has no starter ends at its root) */
uint16_t gc_species_root_for_starter(uint16_t species_id) {
    uint16_t root = species_id;
    for (int depth = 0; depth < 10; depth++) {
        if (gc_starter_cost(root) >= 0) break;
        int16_t pre = gc_species_prevolution(root);
        if (pre <= 0) break;
        root = (uint16_t)pre;
    }
    return root;
}

/* PokemonSpecies.getFullUnlocksData(), as upstream computes it (the dump has it per species: its sexes, variants
 * and obtainable forms, which are not all of the species' forms) */
uint64_t gc_species_full_unlocks_data(uint16_t species_id) {
    if (species_id == 0 || species_id > GC_MAX_SPECIES_ID) return 0;
    for (int i = 0; i < gc_species_dex_info_count; i++) {
        if (gc_species_dex_info_data[i].species == species_id) return gc_species_dex_info_data[i].full_unlocks;
    }
    return 0;
}

int getStarterValueFriendshipCap(int starter_cost) {
    switch (starter_cost) {
    case 1: return 25;
    case 2: return 50;
    case 3: return 75;
    case 4: return 100;
    case 5: return 150;
    case 6: return 200;
    case 7: return 300;
    case 8:
    case 9: return 450;
    case 10:
    default: return 600;
    }
}

void gc_system_set_pokemon_seen(gc_system *sys, const gc_mon *m, bool increment_count, bool trainer) {
    (void)trainer;
    if (!sys || !m || m->b.species == 0 || m->b.species > GC_MAX_SPECIES_ID) return;
    gc_dex_entry *entry = &sys->dex[m->b.species];
    entry->seen_attr |= gc_mon_dex_attr(m);
    if (increment_count) {
        entry->seen_count++;
    }
}

void gc_system_update_species_dex_ivs(gc_system *sys, uint16_t species_id, const uint8_t ivs[6]) {
    if (!sys) return;
    uint16_t id = species_id;
    while (id > 0 && id <= GC_MAX_SPECIES_ID) {
        gc_dex_entry *entry = &sys->dex[id];
        for (int i = 0; i < 6; i++) {
            if (ivs[i] > entry->ivs[i]) entry->ivs[i] = ivs[i];
        }
        int16_t pre = gc_species_prevolution(id);
        if (pre <= 0) break;
        id = (uint16_t)pre;
    }
}

bool gc_system_add_starter_candy(gc_system *sys, uint16_t species_id, int32_t count) {
    if (!sys || species_id == 0 || species_id > GC_MAX_SPECIES_ID) return false;
    gc_starter_entry *s = &sys->starters[species_id];
    if (s->candy_count >= 9999) return false;
    s->candy_count += count;
    if (s->candy_count > 9999) s->candy_count = 9999;
    return true;
}

/* What gameData.setPokemonSpeciesCaught(pokemon, species, incrementCount, fromEgg, showMessage) is asked, and the
 * species whose "added as a starter" message it shows, in order */
typedef struct {
    bool increment_count, from_egg, show_message, daily;
    uint16_t started[GC_CAUGHT_STARTED_MAX];
    int n_started;
} caught_ctx;

static void set_pokemon_species_caught(gc_system *sys, const gc_mon *m, uint16_t species_id, caught_ctx *cx) {
    if (!sys || species_id == 0 || species_id > GC_MAX_SPECIES_ID) return;
    gc_dex_entry *dex_entry = &sys->dex[species_id];
    uint64_t caught_attr_before = dex_entry->caught_attr;
    uint64_t dex_attr = gc_mon_dex_attr(m) & gc_species_full_unlocks_data(species_id);
    dex_entry->caught_attr |= dex_attr;

    /* a caught battle form also marks the base form (getFormAttr(0) = bit 7); Pikachu's marks Pichu's too, Urshifu's
     * and Zygarde's forms go by their own bits, and any other species' by a form change that leads to the form's key */
    int form_index = m->b.form;
    if (form_index > 0) {
        if (m->b.species == 25 && species_id == 172) { /* Pikachu / Pichu */
            dex_entry->caught_attr |= (1ULL << 7);
        }
        if (m->b.species == 892) { /* Urshifu */
            if (form_index == 2) dex_entry->caught_attr |= (1ULL << 7);
            else if (form_index == 3) dex_entry->caught_attr |= (1ULL << 8);
        } else if (m->b.species == 718) { /* Zygarde */
            if (form_index == 4) dex_entry->caught_attr |= (1ULL << 9);
            else if (form_index == 5) dex_entry->caught_attr |= (1ULL << 10);
        } else {
            const char *key = gc_form(m->b.species, form_index)->form_key;
            for (int i = 0; i < gc_form_change_targets_count; i++) {
                if (gc_form_change_targets_data[i].species == species_id && !strcmp(gc_form_change_targets_data[i].form_key, key)) {
                    dex_entry->caught_attr |= (1ULL << 7);
                    break;
                }
            }
        }
    }

    if (gc_starter_cost(species_id) >= 0) {
        sys->starters[species_id].ability_attr |= (1U << m->ability_index);
    }

    /* setPokemonSpeciesCaught reads Pokemon.nature, not getNature(). A Mint's
     * custom override has its separate unlockSpeciesNature call. */
    const uint8_t nature = m->has_custom_nature ? m->base_nature : m->b.nature;
    if (nature <= 24) {
        dex_entry->nature_attr |= (1U << (nature + 1));
    }

    int16_t pre = gc_species_prevolution(species_id);
    bool has_pre = pre > 0;
    bool new_catch = (caught_attr_before == 0);
    bool has_new_attr = (caught_attr_before & dex_attr) != dex_attr;

    if (cx->increment_count) {
        if (cx->from_egg) {
            dex_entry->hatched_count++;
        } else {
            dex_entry->caught_count++;
        }
        /* a Daily Run gives candy for a catch only when it unlocks something new */
        if (!has_pre && (!cx->daily || has_new_attr || cx->from_egg)) {
            int shiny_bonus = m->b.shiny ? 5 * (1 << m->variant) : 1;
            int egg_or_boss_bonus = (cx->from_egg || m->b.is_boss) ? 2 : 1;
            gc_system_add_starter_candy(sys, species_id, shiny_bonus * egg_or_boss_bonus);
        }
    }

    const bool started = new_catch && gc_starter_cost(species_id) >= 0;
    /* a new starter with showMessage = false returns before checkPrevolution; the message comes before it */
    if (started && !cx->show_message) {
        return;
    }
    if (started && cx->n_started < GC_CAUGHT_STARTED_MAX) cx->started[cx->n_started++] = species_id;

    if (has_pre) {
        set_pokemon_species_caught(sys, m, (uint16_t)pre, cx);
    }
}

bool gc_system_set_pokemon_caught_ex(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg,
                                     bool show_message, bool daily, gc_caught_report *report) {
    if (report) report->n_started = 0;
    if (!sys || !m || m->b.species == 0 || m->b.species > GC_MAX_SPECIES_ID) return false;
    uint16_t root = gc_species_root_id(m->b.species);
    if (!increment_count && sys->dex[root].caught_attr == 0) {
        return false;
    }
    caught_ctx cx;
    memset(&cx, 0, sizeof cx);
    cx.increment_count = increment_count;
    cx.from_egg = from_egg;
    cx.show_message = show_message;
    cx.daily = daily;
    set_pokemon_species_caught(sys, m, m->b.species, &cx);
    if (report) {
        report->n_started = cx.n_started;
        memcpy(report->started, cx.started, sizeof cx.started);
    }
    return true;
}

/* The callers that aren't a catch (an egg, a form change, a lifecycle step) ask for showMessage the way
 * incrementCount goes: a count of one is a catch, which shows it, and 0 is an update, which doesn't */
bool gc_system_set_pokemon_caught(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg) {
    return gc_system_set_pokemon_caught_ex(sys, m, increment_count, from_egg, increment_count, false, NULL);
}

int gc_system_add_starter_friendship(gc_system *sys, uint16_t starter_id, int points) {
    if (!sys || points <= 0 || starter_id == 0 || starter_id > GC_MAX_SPECIES_ID) return 0;
    gc_starter_entry *s = &sys->starters[starter_id];
    /* summed in int: the field is 16 bits and the football mode adds a whole match's points at once */
    long total = (long)s->friendship + points;
    int cost = gc_starter_cost(starter_id);
    if (cost < 1) cost = 1;
    int cap = getStarterValueFriendshipCap(cost);
    int candies = 0;
    if (total >= cap) {
        long earned = total / cap;
        if (earned > 9999) earned = 9999;
        bool inc = gc_system_add_starter_candy(sys, starter_id, (int32_t)earned);
        if (inc) {
            total %= cap;
            candies = (int)earned;
        } else {
            total = cap - 1;
        }
    }
    s->friendship = (uint16_t)total;
    return candies;
}

void gc_system_add_friendship(gc_system *sys, const gc_run *run, gc_mon *m, int friendship, bool capped) {
    if (!m) return;
    if (friendship <= 0) {
        int new_f = (int)m->b.friendship + friendship;
        m->b.friendship = (uint8_t)(new_f < 0 ? 0 : new_f);
        return;
    }
    int new_f = (int)m->b.friendship + friendship;
    int final_f = (capped && new_f > 200) ? (m->b.friendship > 200 ? m->b.friendship : 200) : new_f;
    m->b.friendship = (uint8_t)(final_f > 255 ? 255 : final_f);

    if (sys) {
        int mode = run ? run->run.mode : 0;
        int mult = (mode == GAME_MODES_CLASSIC) ? 3 : 1;
        gc_system_add_starter_friendship(sys, gc_species_root_id(m->b.species), friendship * mult);
    }
}

int gc_system_increment_egg_steps(gc_system *sys) {
    if (!sys) return 0;
    int updated = 0;
    for (int i = 0; i < sys->n_eggs; i++) {
        if (sys->eggs[i].hatch_waves > 0) {
            sys->eggs[i].hatch_waves--;
            updated++;
        }
    }
    return updated;
}

bool gc_system_add_egg(gc_system *sys, uint32_t id, int32_t hatch_waves, uint8_t tier, uint16_t species) {
    if (!sys || sys->n_eggs >= GC_MAX_EGGS) return false;
    gc_egg *e = &sys->eggs[sys->n_eggs++];
    memset(e,0,sizeof *e);
    e->id = id;
    e->hatch_waves = hatch_waves;
    e->tier = tier;
    e->species = species;
    return true;
}

bool gc_system_has_voucher_unlock(const gc_system *sys, int trainer_type) {
    if (!sys || trainer_type < 0 || trainer_type >= 512) return false;
    return sys->voucher_unlocks[trainer_type] != 0;
}

bool gc_system_unlock_voucher(gc_system *sys, int trainer_type, int voucher_type) {
    if (!sys || trainer_type < 0 || trainer_type >= 512) return false;
    if (sys->voucher_unlocks[trainer_type]) return false;
    sys->voucher_unlocks[trainer_type] = 1;
    if (voucher_type >= 0 && voucher_type < 4) {
        sys->voucher_counts[voucher_type]++;
    }
    return true;
}

void gc_system_add_voucher_count(gc_system *sys, int voucher_type, int count) {
    if (!sys || voucher_type < 0 || voucher_type >= 4) return;
    sys->voucher_counts[voucher_type] += count;
}

bool gc_system_has_achievement(const gc_system *sys, int achievement_id) {
    if (!sys || achievement_id < 0 || achievement_id >= 256) return false;
    return sys->achievement_flags[achievement_id] != 0;
}

bool gc_system_unlock_achievement(gc_system *sys, int achievement_id) {
    if (!sys || achievement_id < 0 || achievement_id >= 256) return false;
    if (sys->achievement_flags[achievement_id]) return false;
    sys->achievement_flags[achievement_id] = 1;
    return true;
}

void gc_system_add_classic_win(gc_system *sys, uint16_t species_id) {
    if (!sys) return;
    sys->stats.classic_wins++;
    if (species_id > 0 && species_id <= GC_MAX_SPECIES_ID) {
        if (sys->starters[species_id].classic_win_count < 65535) {
            sys->starters[species_id].classic_win_count++;
        }
        uint16_t root = gc_species_root_id(species_id);
        if (root != species_id && root > 0 && root <= GC_MAX_SPECIES_ID) {
            if (sys->starters[root].classic_win_count < 65535) {
                sys->starters[root].classic_win_count++;
            }
        }
    }
}

uint16_t gc_system_get_ribbons(const gc_system *sys, uint16_t species_id) {
    if (!sys || species_id == 0 || species_id > GC_MAX_SPECIES_ID) return 0;
    return sys->starters[species_id].classic_win_count;
}

/* Requested rotation: three unique species per UTC three-hour bucket. Native
 * getPokerusStarters uses the same isolated timestamp seed and registry order,
 * with five per UTC day at the pinned revision. */
void gc_pokerus_starters(uint64_t unix_ms,uint16_t out[3]){
    const uint64_t period=10800000;
    char key[32];snprintf(key,sizeof key,"%llu",(unsigned long long)(unix_ms/period*period));
    pr_seedstr seed;pr_rng rng;pr_seed_from_ascii(&seed,key);pr_rng_sow(&rng,&seed);
    int n=0;while(n<3){
        uint16_t id=gc_starter_costs[pr_rng_seed_int(&rng,gc_starter_cost_count,0)].id;
        bool used=false;for(int k=0;k<n;k++)used|=out[k]==id;
        if(!used)out[n++]=id;
    }
}
