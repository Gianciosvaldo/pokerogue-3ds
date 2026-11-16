/* custom-core: timed events, the schedule model (lorenzo 2026-11-16, "i would like to add events as a feature"; "yearly events span over
 * too long a time period: these should be daily/every two days/1 hour each day depending on how giving they are").
 *
 * Pure like the rest of the module: no globals except the constant table, no I/O, no allocation, no clock, no ambient random numbers.
 * The module only DECIDES, as data: given the console's wall clock it says which events are open and what they change. It has no
 * clock of its own, so the host passes the time in and the tests pass fixed timestamps. Nothing here changes a roll, a reward or a
 * screen: later PRs read cc_tevents_effects() at the moment of each roll (shiny odds, catch rate, luck, candy rate, vouchers, the trainer
 * shiny chance) and the title screen shows cc_tevent_window().
 *
 * Time. Every time is `local_s`: seconds since 1970-01-01 00:00:00 on the console's LOCAL wall clock (not UTC). The module knows no time
 * zones and no daylight saving: a "day" is exactly 86400 of these seconds, so a window never changes length. How the host turns
 * time(NULL) into local_s is the host's job (the 3DS clock is the local time its owner set).
 *
 * Schedule. An event opens on every day D (the day number, days since 1970-01-01) with D mod period_days == phase_day, at start_minute
 * of that day, and stays open length_minutes. The three tiers of the design are three rows of the same shape:
 *   daily, all day            {1, 0, 0, 1440}        mild effects
 *   every two or three days   {2 or 3, phase, 0, 1440}   medium effects: a whole day on, then one or two days off
 *   one hour each day         {1, 0, 1200, 60}       generous effects (20:00 to 21:00)
 * A window may run past midnight, and may be as long as one whole period (then it never closes). It may not be longer, so two windows
 * of one event never overlap.
 *
 * Overlap. Different events overlap freely (a daily one and an hourly one are open together). Upstream has one active event at a time
 * and reads its fields with a default; here every numeric field is "0 = this event does not change it", and when several open events
 * change the same field the strongest wins (the maximum). Nothing stacks, so two Shiny Days at once are still x2. A flag is the OR.
 *
 * What the numbers mean (upstream src/timed-event-manager.ts at the pin 8555c08; the non-event value is what a consumer uses when the
 * combined field is 0):
 *   shiny_encounter_mult   getShinyEncounterMultiplier(): multiplies the wild shiny threshold. Non-event value 1.
 *   shiny_catch_mult       getShinyCatchMultiplier(): catch-rate multiplier of a shiny target. Non-event value SHINY_CATCH_RATE_MULTIPLIER = 2.
 *   candy_mult             getClassicFriendshipMultiplier(): Classic candy friendship multiplier. Non-event value CLASSIC_CANDY_FRIENDSHIP_MULTIPLIER = 3.
 *   luck_boost             getEventLuckBoost(): added to the party luck before the clamp to 14. Non-event value 0.
 *   trainer_shiny_chance   getClassicTrainerShinyChance(): a trainer Pokemon's shiny threshold out of 65536 (6554 = 1 in 10). Non-event value 0.
 *   upgrade_vouchers       getUpgradeUnlockedVouchers(): defeated bosses give the upgraded voucher again. Non-event value false.
 *   rewards                getFixedBattleEventRewards(wave) (upstream's classicWaveRewards): items given in a Classic run after the wave's battle,
 *                          before the shop. Several events' lists are joined without duplicates. Non-event value: none.
 *   luck_species           getEventLuckBoostedSpecies(): each party member of one of these species adds 1 to the luck sum. Lists are joined.
 *   delibirdy_buff         getDelibirdyBuff(): items the Delibird-y encounter hands out as a bonus (one at random among those not at their maximum
 *                          stack; a Voucher when all are). Lists are joined.
 *   flags                  CC_TFX_BOOST_FUSIONS: areFusionsBoosted() (a fusion feeds its candy bars 1/1.5 each instead of 1/2).
 *                          CC_TFX_DELIBIRDY_COMMON: the Delibird-y encounter is a COMMON one instead of GREAT (mysteryEncounterTierChanges).
 *                          CC_TFX_TOWN_SNOW: the Town biome's weather is always snow (weather: {TOWN: {SNOW: 1}}). Flags are the OR. */
#ifndef CC_TEVENTS_H
#define CC_TEVENTS_H

#include <stdint.h>

#include "cc/model.h"

#define CC_TEVENTS_MAX 16u            /* the table can never have more events than this (the active set is a 32-bit mask) */
#define CC_TEVENT_PERIOD_MAX 28u      /* days between two openings, at most */
#define CC_TEVENT_NAME_MAX 24u        /* characters of the shown name (one title-screen line) */
#define CC_TEVENT_MULT_MAX 10u        /* the largest multiplier an event may ask for */
#define CC_TEVENT_LUCK_MAX 14u        /* the game's luck cap */
#define CC_TEVENT_REWARDS_MAX 8u      /* wave rewards of one event, and of all the open events together */
#define CC_TEVENT_SPECIES_MAX 12u     /* luck-boosted species of one event, and of all the open events together */
#define CC_TEVENT_BUFF_MAX 6u         /* Delibird-y bonus items of one event, and of all the open events together */
#define CC_TEVENT_REWARD_WAVE_MAX 199u   /* a Classic run's last wave is 200: its reward is the win, not a shop wave */
#define CC_TEVENT_SPECIES_ID_MAX 10000u  /* a species id, as upstream's SpeciesId (forms included) */
#define CC_TEVENT_TIME_MAX 253402300799ll   /* local_s beyond 9999-12-31 23:59:59 (or before its negative) is refused: no arithmetic can overflow */

typedef struct {
    uint16_t period_days;     /* 1..CC_TEVENT_PERIOD_MAX: 1 = every day, 2 = every other day */
    uint16_t phase_day;       /* 0..period_days-1: it opens on the days whose number is congruent to this modulo period_days */
    uint16_t start_minute;    /* 0..1439: the minute of that day the window opens */
    uint16_t length_minutes;  /* 1..period_days*1440 */
} cc_tevent_schedule;

/* The items an event can hand out (cc_tevent_reward.item, cc_tevent_effects.delibirdy_buff): upstream's modifierTypes keys. 0 = no item. */
typedef enum {
    CC_TITEM_NONE = 0,
    CC_TITEM_SHINY_CHARM,
    CC_TITEM_ABILITY_CHARM,
    CC_TITEM_CATCHING_CHARM,
    CC_TITEM_EXP_CHARM,
    CC_TITEM_SUPER_EXP_CHARM,
    CC_TITEM_HEALING_CHARM,
    CC_TITEM_CANDY_JAR,
    CC_TITEM_MEGA_BRACELET,
    CC_TITEM_DYNAMAX_BAND,
    CC_TITEM_COUNT
} cc_titem;

#define CC_TFX_BOOST_FUSIONS 1u
#define CC_TFX_DELIBIRDY_COMMON 2u
#define CC_TFX_TOWN_SNOW 4u
#define CC_TFX_ALL (CC_TFX_BOOST_FUSIONS | CC_TFX_DELIBIRDY_COMMON | CC_TFX_TOWN_SNOW)

typedef struct {
    uint16_t wave;           /* 1..CC_TEVENT_REWARD_WAVE_MAX; 0 = an unused slot (the list ends at the first unused slot) */
    uint8_t item;            /* a cc_titem other than CC_TITEM_NONE */
} cc_tevent_reward;

typedef struct {
    uint8_t shiny_encounter_mult;    /* 0 = no change, else 1..CC_TEVENT_MULT_MAX */
    uint8_t shiny_catch_mult;        /* 0 = no change, else 1..CC_TEVENT_MULT_MAX */
    uint8_t candy_mult;              /* 0 = no change, else 1..CC_TEVENT_MULT_MAX */
    uint8_t luck_boost;              /* 0..CC_TEVENT_LUCK_MAX */
    uint16_t trainer_shiny_chance;   /* 0 = no change, else the threshold out of 65536 */
    uint8_t upgrade_vouchers;        /* 0 or 1 */
    /* second round (appended, so a six-value initializer still means the first six) */
    cc_tevent_reward rewards[CC_TEVENT_REWARDS_MAX];     /* the list ends at the first slot with wave 0; nothing may follow it */
    uint16_t luck_species[CC_TEVENT_SPECIES_MAX];        /* species ids, the list ends at the first 0; no duplicates */
    uint8_t delibirdy_buff[CC_TEVENT_BUFF_MAX];          /* cc_titem, the list ends at the first CC_TITEM_NONE; no duplicates */
    uint8_t flags;                                       /* CC_TFX_* */
} cc_tevent_effects;

typedef struct {
    const char *id;                  /* "golden_hour": a stable key (letters, digits, '_'), never shown */
    const char *name;                /* "Golden Hour": the title line, at most CC_TEVENT_NAME_MAX characters */
    cc_tevent_schedule when;
    cc_tevent_effects fx;            /* an event that changes nothing is refused (CC_E_CONSISTENCY) */
} cc_tevent;

extern const cc_tevent cc_tevents[];
extern const unsigned cc_tevent_count;

/* ---- checks (never modify their argument) ---- */
cc_status cc_tevent_schedule_validate(const cc_tevent_schedule *s);
cc_status cc_tevent_effects_validate(const cc_tevent_effects *f);
/* id and name present and within their limits, schedule and effects valid, and at least one effect set. */
cc_status cc_tevent_validate(const cc_tevent *e);
/* count <= CC_TEVENTS_MAX (CC_E_COUNT), every event valid, ids pairwise different (CC_E_DUPLICATE). */
cc_status cc_tevents_validate_in(const cc_tevent *ev, unsigned count);
cc_status cc_tevents_validate(void);

/* ---- one schedule ---- */
/* The window of `s` that is open at local_s, or, when none is, the next one to open. *active is 1 for the first case, 0 for the second;
 * *start_s and *end_s are its first second and the second after its last (end_s - start_s == length_minutes * 60). The title screen
 * shows end_s - local_s while open and start_s - local_s while waiting. Any NULL pointer is CC_E_NULL, an invalid schedule or a
 * local_s beyond CC_TEVENT_TIME_MAX is CC_E_RANGE, and nothing is written on an error. */
cc_status cc_tevent_window(const cc_tevent_schedule *s, int64_t local_s, int *active, int64_t *start_s, int64_t *end_s);

/* ---- a table ---- */
/* Bit i is set when ev[i] is open at local_s. A NULL table, a count over CC_TEVENTS_MAX, an invalid event or a local_s out of range
 * returns 0: no event. */
uint32_t cc_tevents_active_in(const cc_tevent *ev, unsigned count, int64_t local_s);
/* The effects of the open events combined as the header says (all zero when none is open or on any of the refusals above). */
cc_tevent_effects cc_tevents_effects_in(const cc_tevent *ev, unsigned count, int64_t local_s);

/* The same on the shipped table. */
uint32_t cc_tevents_active(int64_t local_s);
cc_tevent_effects cc_tevents_effects(int64_t local_s);

#endif
