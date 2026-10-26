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
};
#undef J
#undef RUN

static const uint32_t SET_PILLAR = 1u << GC_JOJO_KARS | 1u << GC_JOJO_WAMUU | 1u << GC_JOJO_ESIDISI;
static const uint32_t SET_P2 = SET_PILLAR | 1u << GC_JOJO_JOSEPH | 1u << GC_JOJO_STROHEIM;
static const uint32_t SET_P3 = 1u << GC_JOJO_JOTARO | 1u << GC_JOJO_DIO | 1u << GC_JOJO_POLNAREFF;
static const uint32_t SET_P4 = 1u << GC_JOJO_KOICHI | 1u << GC_JOJO_KIRA;
static const uint32_t SET_P5 = 1u << GC_JOJO_DIAVOLO | 1u << GC_JOJO_MISTA | 1u << GC_JOJO_RISOTTO;
static const uint32_t SET_P6 = 1u << GC_JOJO_JOLYNE | 1u << GC_JOJO_PUCCI;
static const uint32_t SET_P7 = 1u << GC_JOJO_VALENTINE | 1u << GC_JOJO_GYRO;

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

void gc_achv_wave_reached(gc_system *sys, uint32_t wave)
{
    if (sys && wave > sys->achv.best_wave) sys->achv.best_wave = wave;
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
    default: return false;
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
