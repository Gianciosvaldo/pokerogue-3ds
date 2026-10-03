/* game-core: overall system data across runs (Dex entries, starter data, unlocks).
 * Follows upstream GameData (src/system/game-data.ts). */
#include "gc/system.h"
#include "gc/data.h"
#include "gc/run.h"
#include "internal.h"
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

uint64_t gc_species_full_unlocks_data(uint16_t species_id) {
    if (species_id == 0 || species_id > GC_MAX_SPECIES_ID) return 0;
    uint64_t attr = 1ULL | 2ULL; /* NON_SHINY | SHINY */

    int male_pct = -1;
    bool has_var = false;
    for (int i = 0; i < gc_species_dex_info_count; i++) {
        if (gc_species_dex_info_data[i].species == species_id) {
            male_pct = gc_species_dex_info_data[i].male_percent;
            has_var = gc_species_dex_info_data[i].has_variants;
            break;
        }
    }

    if (male_pct != -1) {
        if (male_pct > 0) attr |= 4ULL;   /* MALE */
        if (male_pct < 100) attr |= 8ULL; /* FEMALE */
    }

    attr |= 16ULL; /* DEFAULT_VARIANT */
    if (has_var) {
        attr |= 32ULL; /* VARIANT_2 */
        attr |= 64ULL; /* VARIANT_3 */
    }

    const pr_species *sp = gc_species(species_id);
    int nforms = sp && sp->nforms > 0 ? sp->nforms : 1;
    for (int f = 0; f < nforms && f < 10; f++) {
        attr |= (1ULL << (7 + f));
    }
    return attr;
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

static void set_pokemon_species_caught(gc_system *sys, const gc_mon *m, uint16_t species_id, bool increment_count, bool from_egg) {
    if (!sys || species_id == 0 || species_id > GC_MAX_SPECIES_ID) return;
    gc_dex_entry *dex_entry = &sys->dex[species_id];
    uint64_t caught_attr_before = dex_entry->caught_attr;
    uint64_t dex_attr = gc_mon_dex_attr(m) & gc_species_full_unlocks_data(species_id);
    dex_entry->caught_attr |= dex_attr;

    int form_index = m->b.form;
    if (form_index > 0) {
        if (m->b.species == 25 && species_id == 172) { /* Pikachu / Pichu */
            dex_entry->caught_attr |= (1ULL << 7);
        } else if (m->b.species == species_id) {
            bool has_fc = false;
            for (int i = 0; i < gc_species_dex_info_count; i++) {
                if (gc_species_dex_info_data[i].species == species_id) {
                    has_fc = gc_species_dex_info_data[i].has_form_changes;
                    break;
                }
            }
            if (m->b.species == 892) { /* Urshifu */
                if (form_index == 2) dex_entry->caught_attr |= (1ULL << 7);
                else if (form_index == 3) dex_entry->caught_attr |= (1ULL << 8);
            } else if (m->b.species == 718) { /* Zygarde */
                if (form_index == 4) dex_entry->caught_attr |= (1ULL << 9);
                else if (form_index == 5) dex_entry->caught_attr |= (1ULL << 10);
            } else if (has_fc) {
                dex_entry->caught_attr |= (1ULL << 7);
            }
        }
    }

    if (gc_starter_cost(species_id) >= 0) {
        sys->starters[species_id].ability_attr |= (1U << m->ability_index);
    }

    if (m->b.nature <= 24) {
        dex_entry->nature_attr |= (1U << (m->b.nature + 1));
    }

    int16_t pre = gc_species_prevolution(species_id);
    bool has_pre = pre > 0;
    bool new_catch = (caught_attr_before == 0);

    if (increment_count) {
        if (from_egg) {
            dex_entry->hatched_count++;
        } else {
            dex_entry->caught_count++;
        }
        if (!has_pre) {
            int shiny_bonus = m->b.shiny ? 5 * (1 << m->variant) : 1;
            int egg_or_boss_bonus = (from_egg || m->b.is_boss) ? 2 : 1;
            gc_system_add_starter_candy(sys, species_id, shiny_bonus * egg_or_boss_bonus);
        }
    }

    /* Upstream line 1738: if a species is a starter and newCatch is true,
     * setPokemonCaught with showMessage = false (increment_count = false) returns early
     * before checkPrevolution */
    if (new_catch && gc_starter_cost(species_id) >= 0 && !increment_count) {
        return;
    }

    if (has_pre) {
        set_pokemon_species_caught(sys, m, (uint16_t)pre, increment_count, from_egg);
    }
}

bool gc_system_set_pokemon_caught(gc_system *sys, const gc_mon *m, bool increment_count, bool from_egg) {
    if (!sys || !m || m->b.species == 0 || m->b.species > GC_MAX_SPECIES_ID) return false;
    uint16_t root = gc_species_root_id(m->b.species);
    if (!increment_count && sys->dex[root].caught_attr == 0) {
        return false;
    }
    set_pokemon_species_caught(sys, m, m->b.species, increment_count, from_egg);
    return true;
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
        int candy_f_amount = friendship * mult;
        uint16_t starter_id = gc_species_root_id(m->b.species);
        if (starter_id > 0 && starter_id <= GC_MAX_SPECIES_ID) {
            gc_starter_entry *s = &sys->starters[starter_id];
            s->friendship += candy_f_amount;
            int cost = gc_starter_cost(starter_id);
            if (cost < 1) cost = 1;
            int cap = getStarterValueFriendshipCap(cost);
            if (s->friendship >= cap) {
                int candies = s->friendship / cap;
                bool inc = gc_system_add_starter_candy(sys, starter_id, candies);
                if (inc) {
                    s->friendship %= cap;
                } else {
                    s->friendship = cap - 1;
                }
            }
        }
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
