/* The custom achievements: see gc/achv.h. */
#include <stdio.h>

#include "internal.h"
#include "gc/achv.h"
#include "gc/gacha.h"

#define V(t, n) GC_AREW_VOUCHER, t, n
#define EGG(t) GC_AREW_EGG, t, 1
#define SHINY(t) GC_AREW_SHINY_EGG, t, 1
#define J GC_ACHV_GROUP_JOJO
#define RUN GC_ACHV_GROUP_RUNS
#define COL GC_ACHV_GROUP_COLLECTION
#define EGGS GC_ACHV_GROUP_EGGS
#define TR GC_ACHV_GROUP_TRAINERS
#define FB GC_ACHV_GROUP_FOOTBALL
/* Ids follow the list order: the 17 challengers (GC_JOJO_*), then the sets and counts. */
const gc_achv_def gc_achv_defs[GC_ACHV_COUNT] = {
    {"Kars", "Defeat Kars.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Gyro Zeppeli", "Defeat Gyro Zeppeli.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Diavolo", "Defeat Diavolo.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Yoshikage Kira", "Defeat Yoshikage Kira.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Enrico Pucci", "Defeat Enrico Pucci.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Jolyne Cujoh", "Defeat Jolyne Cujoh.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Joseph Joestar", "Defeat Joseph Joestar.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Jotaro Kujo", "Defeat Jotaro Kujo.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"DIO", "Defeat DIO.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Koichi Hirose", "Defeat Koichi Hirose.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Guido Mista", "Defeat Guido Mista.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Wamuu", "Defeat Wamuu.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Esidisi", "Defeat Esidisi.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Funny Valentine", "Defeat Funny Valentine.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Jean Pierre Polnareff", "Defeat Jean Pierre Polnareff.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Risotto Nero", "Defeat Risotto Nero.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"Rudolf von Stroheim", "Defeat Rudolf von Stroheim.", J, V(GC_VOUCHER_REGULAR, 1)},
    {"The Pillar Men", "Defeat Kars, Wamuu and Esidisi.", J, V(GC_VOUCHER_PLUS, 1)},
    {"Battle Tendency", "Defeat Joseph, Kars, Wamuu, Esidisi and Stroheim.", J, V(GC_VOUCHER_PLUS, 1)},
    {"Stardust Crusaders", "Defeat Jotaro, DIO and Polnareff.", J, V(GC_VOUCHER_PLUS, 1)},
    {"Diamond Is Unbreakable", "Defeat Koichi and Kira.", J, V(GC_VOUCHER_REGULAR, 2)},
    {"Golden Wind", "Defeat Diavolo, Mista and Risotto.", J, V(GC_VOUCHER_PLUS, 1)},
    {"Stone Ocean", "Defeat Jolyne and Pucci.", J, V(GC_VOUCHER_REGULAR, 2)},
    {"Steel Ball Run", "Defeat Valentine and Gyro.", J, V(GC_VOUCHER_REGULAR, 2)},
    {"Five Faces", "Defeat 5 different challengers.", J, V(GC_VOUCHER_REGULAR, 2)},
    {"Ten Faces", "Defeat 10 different challengers.", J, V(GC_VOUCHER_PLUS, 1)},
    {"Every Face", "Defeat all 17 different challengers.", J, SHINY(2)},
    {"Hat Trick", "Defeat 3 challengers in one run.", J, V(GC_VOUCHER_REGULAR, 2)},
    {"Hundred Duels", "Win 100 challenger battles.", J, EGG(3)},
    {"Untouched", "Defeat a challenger without any of your Pokemon fainting.", J, V(GC_VOUCHER_PLUS, 1)},
    /* 30..43: the runs */
    {"Victor", "Win a Classic run.", RUN, V(GC_VOUCHER_PLUS, 1)},
    {"Five Victories", "Win 5 Classic runs.", RUN, V(GC_VOUCHER_PLUS, 1)},
    {"Ten Victories", "Win 10 Classic runs.", RUN, V(GC_VOUCHER_PREMIUM, 1)},
    {"Twenty-Five Victories", "Win 25 Classic runs.", RUN, EGG(3)},
    {"Fifty Victories", "Win 50 Classic runs.", RUN, SHINY(2)},
    {"Custom Victor", "Win a Custom run.", RUN, V(GC_VOUCHER_PLUS, 1)},
    {"Custom Veteran", "Win 10 Custom runs.", RUN, V(GC_VOUCHER_PREMIUM, 1)},
    {"Wave 50", "Reach wave 50 in a run.", RUN, V(GC_VOUCHER_REGULAR, 2)},
    {"Wave 100", "Reach wave 100 in a run.", RUN, V(GC_VOUCHER_PLUS, 1)},
    {"Wave 150", "Reach wave 150 in a run.", RUN, V(GC_VOUCHER_PREMIUM, 1)},
    {"Wave 190", "Reach wave 190 in a run.", RUN, EGG(2)},
    {"Fresh Faces", "Win a Classic run with six Pokemon that can still evolve.", RUN, V(GC_VOUCHER_PREMIUM, 1)},
    {"Lone Hero", "Win a Classic run in which only one Pokemon ever fought.", RUN, V(GC_VOUCHER_PREMIUM, 1)},
    {"Ball-Free", "Win a Classic run without throwing a Poke Ball.", RUN, V(GC_VOUCHER_PLUS, 1)},
    /* 44..59: the collection */
    {"Collector", "Catch 10 different species.", COL, V(GC_VOUCHER_REGULAR, 1)},
    {"Keen Collector", "Catch 50 different species.", COL, V(GC_VOUCHER_REGULAR, 2)},
    {"Seasoned Collector", "Catch 100 different species.", COL, V(GC_VOUCHER_PLUS, 1)},
    {"Master Collector", "Catch 250 different species.", COL, V(GC_VOUCHER_PREMIUM, 1)},
    {"Grand Collector", "Catch 500 different species.", COL, V(GC_VOUCHER_GOLDEN, 1)},
    {"Gotta Catch Them All", "Catch every species you can catch.", COL, EGG(3)},
    {"Something Shiny", "Catch your first shiny.", COL, V(GC_VOUCHER_REGULAR, 2)},
    {"Glittering Five", "Own shinies of 5 different species.", COL, V(GC_VOUCHER_PLUS, 1)},
    {"Glittering Twenty-Five", "Own shinies of 25 different species.", COL, SHINY(2)},
    {"Radiant", "Own a shiny of the rarest kind (tier 3).", COL, V(GC_VOUCHER_PREMIUM, 1)},
    {"Legend Hunter", "Catch your first legendary.", COL, V(GC_VOUCHER_REGULAR, 2)},
    {"Legend Chaser", "Catch 5 different legendaries.", COL, V(GC_VOUCHER_PLUS, 1)},
    {"All the Legends", "Catch every legendary you can catch.", COL, EGG(3)},
    {"Mythical Find", "Catch your first mythical.", COL, V(GC_VOUCHER_PLUS, 1)},
    {"Generation Complete", "Catch every species of one generation.", COL, V(GC_VOUCHER_PLUS, 1)},
    {"Complete Pokedex", "Catch every species of every generation.", COL, SHINY(2)},
    /* 60..67: the eggs */
    {"First Hatch", "Hatch an egg.", EGGS, V(GC_VOUCHER_REGULAR, 1)},
    {"Ten Hatches", "Hatch 10 eggs.", EGGS, V(GC_VOUCHER_REGULAR, 1)},
    {"Fifty Hatches", "Hatch 50 eggs.", EGGS, V(GC_VOUCHER_REGULAR, 2)},
    {"Hundred Hatches", "Hatch 100 eggs.", EGGS, V(GC_VOUCHER_PLUS, 1)},
    {"Five Hundred Hatches", "Hatch 500 eggs.", EGGS, V(GC_VOUCHER_PREMIUM, 1)},
    {"Epic Hatch", "Hatch an Epic egg.", EGGS, V(GC_VOUCHER_REGULAR, 2)},
    {"Legendary Hatch", "Hatch a Legendary egg.", EGGS, V(GC_VOUCHER_PLUS, 1)},
    {"Shiny Hatch", "Hatch a shiny from an egg.", EGGS, V(GC_VOUCHER_REGULAR, 2)},
    /* 68..90: the trainers (Gym Leaders, Elite Four, Champions, then a region at a time) */
    {"Gym Challenger", "Defeat a Gym Leader.", TR, V(GC_VOUCHER_REGULAR, 1)},
    {"Badge Collector", "Defeat 8 different Gym Leaders.", TR, V(GC_VOUCHER_REGULAR, 2)},
    {"Gym Veteran", "Defeat 25 different Gym Leaders.", TR, V(GC_VOUCHER_PLUS, 1)},
    {"Gym Master", "Defeat 50 different Gym Leaders.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Gym Legend", "Defeat every Gym Leader.", TR, V(GC_VOUCHER_GOLDEN, 1)},
    {"Elite Challenger", "Defeat an Elite Four member.", TR, V(GC_VOUCHER_REGULAR, 2)},
    {"Elite Squad", "Defeat 4 different Elite Four members.", TR, V(GC_VOUCHER_PLUS, 1)},
    {"Elite Veteran", "Defeat 16 different Elite Four members.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Elite Legend", "Defeat every Elite Four member.", TR, EGG(2)},
    {"Champion Slayer", "Defeat a Champion.", TR, V(GC_VOUCHER_PLUS, 1)},
    {"Champion Collector", "Defeat 5 different Champions.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Champion of Champions", "Defeat every Champion.", TR, EGG(2)},
    {"Kanto Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Kanto.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Johto Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Johto.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Hoenn Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Hoenn.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Sinnoh Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Sinnoh.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Unova Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Unova.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Kalos Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Kalos.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Alola Conqueror", "Defeat every Elite Four member and Champion of Alola (it has no Gym Leaders).", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Galar Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Galar.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Paldea Conqueror", "Defeat every Gym Leader, Elite Four member and the Champion of Paldea.", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Blueberry Conqueror", "Defeat every Elite Four member and Champion of Blueberry (it has no Gym Leaders).", TR, V(GC_VOUCHER_PREMIUM, 1)},
    {"Grand Tour", "Conquer all 10 regions.", TR, EGG(2)},
    /* 91..99: the football playoffs */
    {"Kickoff", "Win a Football Playoffs match.", FB, V(GC_VOUCHER_REGULAR, 1)},
    {"Wild Card", "Win 10 Football Playoffs matches.", FB, V(GC_VOUCHER_PLUS, 1)},
    {"Playoff Regular", "Win 50 Football Playoffs matches.", FB, V(GC_VOUCHER_PREMIUM, 1)},
    {"League Legend", "Win 200 Football Playoffs matches.", FB, EGG(2)},
    {"Football Champion", "Win the Football Playoffs title.", FB, V(GC_VOUCHER_PLUS, 1)},
    {"Dynasty", "Win the Football Playoffs title 3 times.", FB, V(GC_VOUCHER_PREMIUM, 1)},
    {"Hall of Famer", "Win the Football Playoffs title 10 times.", FB, V(GC_VOUCHER_GOLDEN, 1)},
    {"Shutout", "Win a Football Playoffs match without conceding a point.", FB, V(GC_VOUCHER_REGULAR, 2)},
    {"Blowout", "Win a Football Playoffs match by 21 points or more.", FB, V(GC_VOUCHER_REGULAR, 2)},
};
#undef J
#undef RUN
#undef COL
#undef EGGS
#undef TR
#undef FB

static const uint32_t SET_PILLAR = 1u << GC_JOJO_KARS | 1u << GC_JOJO_WAMUU | 1u << GC_JOJO_ESIDISI;
static const uint32_t SET_P2 = SET_PILLAR | 1u << GC_JOJO_JOSEPH | 1u << GC_JOJO_STROHEIM;
static const uint32_t SET_P3 = 1u << GC_JOJO_JOTARO | 1u << GC_JOJO_DIO | 1u << GC_JOJO_POLNAREFF;
static const uint32_t SET_P4 = 1u << GC_JOJO_KOICHI | 1u << GC_JOJO_KIRA;
static const uint32_t SET_P5 = 1u << GC_JOJO_DIAVOLO | 1u << GC_JOJO_MISTA | 1u << GC_JOJO_RISOTTO;
static const uint32_t SET_P6 = 1u << GC_JOJO_JOLYNE | 1u << GC_JOJO_PUCCI;
static const uint32_t SET_P7 = 1u << GC_JOJO_VALENTINE | 1u << GC_JOJO_GYRO;

/* ---- the trainers: TrainerType values, the groups of pr_enums.h (Gym Leaders 200.., Elite Four 300.., Champions 350..) ---- */
#define GYM_FIRST TRAINER_TYPE_BROCK
#define GYM_LAST TRAINER_TYPE_GRUSHA
#define ELITE_FIRST TRAINER_TYPE_LORELEI
#define ELITE_LAST TRAINER_TYPE_DRAYTON
#define CHAMPION_FIRST TRAINER_TYPE_BLUE
#define CHAMPION_LAST TRAINER_TYPE_KIERAN

/* The regions, in the order of GC_REGION_*: run-core's e4_fixed table (a run's Elite Four and Champion come from one column of it) and the
 * Gym Leaders' enum ranges. Bruno is in two Elite Fours (Kanto and Johto). */
static const gc_region REGIONS[GC_REGION_COUNT] = {
    {"Kanto", TRAINER_TYPE_BROCK, TRAINER_TYPE_GIOVANNI, {TRAINER_TYPE_LORELEI, TRAINER_TYPE_BRUNO, TRAINER_TYPE_AGATHA, TRAINER_TYPE_LANCE}, {TRAINER_TYPE_BLUE}},
    {"Johto", TRAINER_TYPE_FALKNER, TRAINER_TYPE_CLAIR, {TRAINER_TYPE_WILL, TRAINER_TYPE_KOGA, TRAINER_TYPE_BRUNO, TRAINER_TYPE_KAREN}, {TRAINER_TYPE_RED, TRAINER_TYPE_LANCE_CHAMPION}},
    {"Hoenn", TRAINER_TYPE_ROXANNE, TRAINER_TYPE_JUAN, {TRAINER_TYPE_SIDNEY, TRAINER_TYPE_PHOEBE, TRAINER_TYPE_GLACIA, TRAINER_TYPE_DRAKE}, {TRAINER_TYPE_STEVEN, TRAINER_TYPE_WALLACE}},
    {"Sinnoh", TRAINER_TYPE_ROARK, TRAINER_TYPE_VOLKNER, {TRAINER_TYPE_AARON, TRAINER_TYPE_BERTHA, TRAINER_TYPE_FLINT, TRAINER_TYPE_LUCIAN}, {TRAINER_TYPE_CYNTHIA}},
    {"Unova", TRAINER_TYPE_CILAN, TRAINER_TYPE_MARLON, {TRAINER_TYPE_SHAUNTAL, TRAINER_TYPE_MARSHAL, TRAINER_TYPE_GRIMSLEY, TRAINER_TYPE_CAITLIN}, {TRAINER_TYPE_ALDER, TRAINER_TYPE_IRIS}},
    {"Kalos", TRAINER_TYPE_VIOLA, TRAINER_TYPE_WULFRIC, {TRAINER_TYPE_MALVA, TRAINER_TYPE_SIEBOLD, TRAINER_TYPE_WIKSTROM, TRAINER_TYPE_DRASNA}, {TRAINER_TYPE_DIANTHA}},
    {"Alola", 0, 0, {TRAINER_TYPE_HALA, TRAINER_TYPE_MOLAYNE, TRAINER_TYPE_OLIVIA, TRAINER_TYPE_ACEROLA, TRAINER_TYPE_KAHILI}, {TRAINER_TYPE_KUKUI, TRAINER_TYPE_HAU}},
    {"Galar", TRAINER_TYPE_MILO, TRAINER_TYPE_RAIHAN, {TRAINER_TYPE_MARNIE_ELITE, TRAINER_TYPE_BEDE_ELITE, TRAINER_TYPE_NESSA_ELITE, TRAINER_TYPE_BEA_ELITE, TRAINER_TYPE_ALLISTER_ELITE, TRAINER_TYPE_RAIHAN_ELITE}, {TRAINER_TYPE_LEON, TRAINER_TYPE_MUSTARD}},
    {"Paldea", TRAINER_TYPE_KATY, TRAINER_TYPE_GRUSHA, {TRAINER_TYPE_RIKA, TRAINER_TYPE_POPPY, TRAINER_TYPE_LARRY_ELITE, TRAINER_TYPE_HASSEL}, {TRAINER_TYPE_GEETA, TRAINER_TYPE_NEMONA}},
    {"Blueberry", 0, 0, {TRAINER_TYPE_CRISPIN, TRAINER_TYPE_AMARYS, TRAINER_TYPE_LACEY, TRAINER_TYPE_DRAYTON}, {TRAINER_TYPE_KIERAN}},
};

const gc_region *gc_achv_region(int region) { return region >= 0 && region < GC_REGION_COUNT ? &REGIONS[region] : NULL; }

static bool beaten(const gc_system *sys, int trainer) { return gc_system_has_voucher_unlock(sys, trainer); }

/* how many of the trainers first..last (TrainerType) have been beaten, and how many there are */
static uint32_t beaten_in(const gc_system *sys, int first, int last, uint32_t *total)
{
    uint32_t n = 0;
    for (int t = first; t <= last; t++) n += beaten(sys, t);
    if (total) *total = (uint32_t)(last - first + 1);
    return n;
}

/* progress through a region: Gym Leaders, Elite Four and Champions beaten of all of them */
static void region_progress(const gc_system *sys, int region, uint32_t *have, uint32_t *need)
{
    const gc_region *r = &REGIONS[region];
    uint32_t h = 0, n = 0;
    if (r->gym_first) h += beaten_in(sys, r->gym_first, r->gym_last, &n);
    for (int i = 0; r->elite[i]; i++, n++) h += beaten(sys, r->elite[i]);
    /* the Champion is one slot: a run meets one of a region's Champions (a Red/Lance coin), so beating either one counts, as upstream plays it */
    bool champ = false;
    for (int i = 0; r->champion[i]; i++) champ = champ || beaten(sys, r->champion[i]);
    h += champ;
    n++;
    *have = h;
    *need = n;
}

static uint32_t regions_done(const gc_system *sys)
{
    uint32_t done = 0;
    for (int r = 0; r < GC_REGION_COUNT; r++) {
        uint32_t h, n;
        region_progress(sys, r, &h, &n);
        done += h >= n;
    }
    return done;
}

static int popcount32(uint32_t v)
{
    int n = 0;
    for (; v; v &= v - 1) n++;
    return n;
}

void gc_achv_jojo_won(gc_system *sys, int jojo, uint32_t run_tag, bool clean)
{
    if (!sys || jojo < 0 || jojo >= GC_JOJO_COUNT) return;
    gc_achv_progress *a = &sys->achv;
    a->jojo_beaten |= 1u << jojo;
    if (a->jojo_battles < UINT32_MAX) a->jojo_battles++;
    if (clean && a->jojo_clean < UINT32_MAX) a->jojo_clean++;
    if (a->run_tag != run_tag || a->run_jojo == 0) {
        a->run_tag = run_tag;
        a->run_jojo = 0;
    }
    if (a->run_jojo < 255) a->run_jojo++;
    if (a->run_jojo > a->best_run_jojo) a->best_run_jojo = a->run_jojo;
}

/* ---- the Pokedex counts ---- */
#define DEX_SHINY 2ull
#define DEX_VARIANT_3 64ull
#define GEN_COUNT 9
static const uint16_t GEN_LAST[GEN_COUNT] = {151, 251, 386, 493, 649, 721, 809, 905, 1025};

/* the species a new profile starts with already in its Pokedex: they are not catches, so "catch N species" does not count them */
static bool starts_caught(int id)
{
    static uint8_t known[1026];
    static bool built;
    if (!built) {
        static gc_system fresh;
        gc_system_init_defaults(&fresh);
        for (int i = 1; i <= 1025; i++) known[i] = fresh.dex[i].caught_attr != 0;
        built = true;
    }
    return id >= 1 && id <= 1025 && known[id];
}

typedef struct {
    uint32_t caught, catchable, caught_catchable, shiny, shiny3, legendary, legendary_total, legendary_catchable, mythical, gens_done;
} dex_counts;

static void count_dex(const gc_system *sys, dex_counts *c)
{
    memset(c, 0, sizeof *c);
    int gen = 0;
    bool whole = true;
    for (int i = 0; i < pr_species_gen_count; i++) {
        const pr_species_gen *g = &pr_species_gens[i];
        if (g->id < 1 || g->id > 1025 || g->id > GC_MAX_SPECIES_ID) continue;
        while (gen < GEN_COUNT && g->id > GEN_LAST[gen]) {
            if (whole) c->gens_done++;
            whole = true;
            gen++;
        }
        const gc_dex_entry *d = &sys->dex[g->id];
        const bool caught = d->caught_attr != 0;
        const bool leg = g->legendary || g->sub_legendary;
        if (caught && !starts_caught(g->id)) c->caught++;
        if (g->catchable) {
            c->catchable++;
            if (caught) c->caught_catchable++;
            if (!caught) whole = false;
            if (leg) {
                c->legendary_total++;
                if (caught) c->legendary_catchable++;
            }
        }
        if (d->caught_attr & DEX_SHINY) c->shiny++;
        if ((d->caught_attr & DEX_SHINY) && (d->caught_attr & DEX_VARIANT_3)) c->shiny3++;
        if (caught && leg) c->legendary++;
        if (caught && g->mythical) c->mythical++;
    }
    if (gen < GEN_COUNT && whole) c->gens_done++;
}

void gc_achv_wave_reached(gc_system *sys, uint32_t wave)
{
    if (sys && wave > sys->achv.best_wave) sys->achv.best_wave = wave;
}

void gc_achv_egg_hatched(gc_system *sys, int tier, bool shiny)
{
    if (!sys) return;
    if (tier == 2) sys->achv.hatch_feats |= GC_HATCH_EPIC;
    if (tier == 3) sys->achv.hatch_feats |= GC_HATCH_LEGENDARY;
    if (shiny) sys->achv.hatch_feats |= GC_HATCH_SHINY;
}

static void add_sat(uint32_t *v, unsigned n) { *v = n > UINT32_MAX - *v ? UINT32_MAX : *v + n; }

void gc_achv_football_won(gc_system *sys, unsigned wins, unsigned titles, unsigned shutouts, unsigned blowouts)
{
    if (!sys) return;
    add_sat(&sys->achv.fb_wins, wins);
    add_sat(&sys->achv.fb_titles, titles);
    add_sat(&sys->achv.fb_shutouts, shutouts);
    add_sat(&sys->achv.fb_blowouts, blowouts);
}

void gc_achv_run_won(gc_system *sys, bool custom, uint8_t feats)
{
    if (!sys) return;
    if (custom && sys->achv.custom_wins < UINT32_MAX) sys->achv.custom_wins++;
    sys->achv.feats |= feats;
}

bool gc_achv_progress_of(const gc_system *sys, int id, uint32_t *have, uint32_t *need)
{
    if (!sys || id < 0 || id >= GC_ACHV_COUNT) return false;
    const gc_achv_progress *a = &sys->achv;
    uint32_t h, n;
    switch (id) {
    case 17: h = (uint32_t)popcount32(a->jojo_beaten & SET_PILLAR); n = 3; break;
    case 18: h = (uint32_t)popcount32(a->jojo_beaten & SET_P2); n = 5; break;
    case 19: h = (uint32_t)popcount32(a->jojo_beaten & SET_P3); n = 3; break;
    case 20: h = (uint32_t)popcount32(a->jojo_beaten & SET_P4); n = 2; break;
    case 21: h = (uint32_t)popcount32(a->jojo_beaten & SET_P5); n = 3; break;
    case 22: h = (uint32_t)popcount32(a->jojo_beaten & SET_P6); n = 2; break;
    case 23: h = (uint32_t)popcount32(a->jojo_beaten & SET_P7); n = 2; break;
    case 24: h = (uint32_t)popcount32(a->jojo_beaten); n = 5; break;
    case 25: h = (uint32_t)popcount32(a->jojo_beaten); n = 10; break;
    case 26: h = (uint32_t)popcount32(a->jojo_beaten); n = GC_JOJO_COUNT; break;
    case 27: h = a->best_run_jojo; n = 3; break;
    case 28: h = a->jojo_battles; n = 100; break;
    case 30: h = sys->stats.classic_wins; n = 1; break;
    case 31: h = sys->stats.classic_wins; n = 5; break;
    case 32: h = sys->stats.classic_wins; n = 10; break;
    case 33: h = sys->stats.classic_wins; n = 25; break;
    case 34: h = sys->stats.classic_wins; n = 50; break;
    case 35: h = a->custom_wins; n = 1; break;
    case 36: h = a->custom_wins; n = 10; break;
    case 37: h = a->best_wave; n = 50; break;
    case 38: h = a->best_wave; n = 100; break;
    case 39: h = a->best_wave; n = 150; break;
    case 40: h = a->best_wave; n = 190; break;
    case 60: h = sys->stats.total_hatches; n = 1; break;
    case 61: h = sys->stats.total_hatches; n = 10; break;
    case 62: h = sys->stats.total_hatches; n = 50; break;
    case 63: h = sys->stats.total_hatches; n = 100; break;
    case 64: h = sys->stats.total_hatches; n = 500; break;
    case 68: case 69: case 70: case 71: case 72: {
        static const uint32_t NEED[5] = {1, 8, 25, 50, 0};
        uint32_t total;
        h = beaten_in(sys, GYM_FIRST, GYM_LAST, &total);
        n = id == 72 ? total : NEED[id - 68];
        break;
    }
    case 73: case 74: case 75: case 76: {
        static const uint32_t NEED[4] = {1, 4, 16, 0};
        uint32_t total;
        h = beaten_in(sys, ELITE_FIRST, ELITE_LAST, &total);
        n = id == 76 ? total : NEED[id - 73];
        break;
    }
    case 77: case 78: case 79: {
        static const uint32_t NEED[3] = {1, 5, 0};
        uint32_t total;
        h = beaten_in(sys, CHAMPION_FIRST, CHAMPION_LAST, &total);
        n = id == 79 ? total : NEED[id - 77];
        break;
    }
    case GC_ACHV_GRAND_TOUR: h = regions_done(sys); n = GC_REGION_COUNT; break;
    case 91: h = a->fb_wins; n = 1; break;
    case 92: h = a->fb_wins; n = 10; break;
    case 93: h = a->fb_wins; n = 50; break;
    case 94: h = a->fb_wins; n = 200; break;
    case 95: h = a->fb_titles; n = 1; break;
    case 96: h = a->fb_titles; n = 3; break;
    case 97: h = a->fb_titles; n = 10; break;
    case 98: h = a->fb_shutouts; n = 1; break;
    case 99: h = a->fb_blowouts; n = 1; break;
    default:
        if (id >= GC_ACHV_REGION_FIRST && id < GC_ACHV_REGION_FIRST + GC_REGION_COUNT) {
            region_progress(sys, id - GC_ACHV_REGION_FIRST, &h, &n);
            break;
        }
        if (id < 44 || id >= 68) return false; /* 91.. are cases above */
        {
            dex_counts c;
            count_dex(sys, &c);
            switch (id) {
            case 44: h = c.caught; n = 10; break;
            case 45: h = c.caught; n = 50; break;
            case 46: h = c.caught; n = 100; break;
            case 47: h = c.caught; n = 250; break;
            case 48: h = c.caught; n = 500; break;
            case 49: h = c.caught_catchable; n = c.catchable; break;
            case 50: h = c.shiny; n = 1; break;
            case 51: h = c.shiny; n = 5; break;
            case 52: h = c.shiny; n = 25; break;
            case 53: h = c.shiny3; n = 1; break;
            case 54: h = c.legendary; n = 1; break;
            case 55: h = c.legendary; n = 5; break;
            case 56: h = c.legendary_catchable; n = c.legendary_total; break;
            case 57: h = c.mythical; n = 1; break;
            case 58: h = c.gens_done; n = 1; break;
            case 59: h = c.gens_done; n = GEN_COUNT; break;
            default: return false;
            }
        }
        break;
    }
    if (h > n) h = n;
    if (have) *have = h;
    if (need) *need = n;
    return true;
}

static bool met(const gc_system *sys, int id)
{
    const gc_achv_progress *a = &sys->achv;
    if (id < GC_JOJO_COUNT) return (a->jojo_beaten >> id & 1u) != 0;
    if (id == 29) return a->jojo_clean > 0;
    if (id == 65) return (a->hatch_feats & GC_HATCH_EPIC) != 0;
    if (id == 66) return (a->hatch_feats & GC_HATCH_LEGENDARY) != 0;
    if (id == 67) return (a->hatch_feats & GC_HATCH_SHINY) != 0;
    if (id == 41) return (a->feats & GC_FEAT_SIX_UNEVOLVED) != 0;
    if (id == 42) return (a->feats & GC_FEAT_SOLO) != 0;
    if (id == 43) return (a->feats & GC_FEAT_NO_BALL) != 0;
    uint32_t h, n;
    return gc_achv_progress_of(sys, id, &h, &n) && h >= n;
}

bool gc_achv_unlocked(const gc_system *sys, int id)
{
    return sys && id >= 0 && id < GC_ACHV_COUNT && sys->achievement_flags[id] != 0;
}

const char *gc_achv_reward_name(const gc_achv_def *d)
{
    static const char *const one[] = {"1 Egg Voucher", "1 Egg Voucher Plus", "1 Egg Voucher Premium", "1 Egg Voucher Golden"};
    static const char *const two[] = {"2 Egg Vouchers", "2 Egg Vouchers Plus", "2 Egg Vouchers Premium", "2 Egg Vouchers Golden"};
    static const char *const eggs[] = {"1 Common Egg", "1 Rare Egg", "1 Epic Egg", "1 Legendary Egg"};
    if (!d) return "";
    switch (d->reward) {
    case GC_AREW_VOUCHER: return d->arg < 4 ? (d->n == 1 ? one[d->arg] : two[d->arg]) : "";
    case GC_AREW_EGG: return d->arg < 4 ? eggs[d->arg] : "";
    case GC_AREW_SHINY_EGG: return "1 Egg with a guaranteed tier 3 shiny";
    }
    return "";
}

/* the reward goes to the profile; false when it cannot be given now (the egg list is full) and nothing changed */
static bool give(gc_system *sys, int id, const char *seed_text, uint64_t now_ms)
{
    const gc_achv_def *d = &gc_achv_defs[id];
    switch (d->reward) {
    case GC_AREW_VOUCHER:
        gc_system_add_voucher_count(sys, d->arg, d->n);
        return true;
    case GC_AREW_EGG:
    case GC_AREW_SHINY_EGG: {
        char seed[25];
        /* 24 characters of the run's text and the achievement, so each reward egg has its own draw */
        snprintf(seed, sizeof seed, "%.20s%03d", seed_text ? seed_text : "achievement", id);
        return gc_system_gift_egg(sys, d->arg, d->reward == GC_AREW_SHINY_EGG, seed, now_ms);
    }
    }
    return false;
}

int gc_achv_evaluate(gc_system *sys, const char *seed_text, uint64_t now_ms, gc_achv_new *out)
{
    if (out) out->n = 0;
    if (!sys) return 0;
    int n = 0;
    for (int id = 0; id < GC_ACHV_COUNT; id++) {
        if (sys->achievement_flags[id] || !met(sys, id)) continue;
        if (!give(sys, id, seed_text, now_ms)) continue;
        sys->achievement_flags[id] = 1;
        if (out && out->n < GC_ACHV_MAX_NEW) out->id[out->n++] = (uint8_t)id;
        n++;
    }
    return n;
}
