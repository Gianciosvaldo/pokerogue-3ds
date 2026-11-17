/* custom-core: the timed events (data; see cc/tevents.h).
 *
 * DRAFT. lorenzo (2026-11-16) chose his own events instead of upstream's, with three tiers by how giving an event is: daily (all day),
 * every two or three days, one hour each day. He asked for Shiny Day every 3 days and Voucher Plus every 2, and for the events to take
 * their effects from upstream's. The rows below are the thread's proposal for him to cut and change; he has not approved the whole
 * list. The first six are wired into the game (shiny odds, shiny catch, candy, luck, vouchers); the seven of the second round are wired one
 * effect group at a time. Times are the console's local time.
 *
 * Numbers come from upstream's own events at the pin 8555c08 (src/data/balance/timed-events.ts): shinyEncounterMultiplier 2,
 * shinyCatchMultiplier 3 (the non-event value is 2), classicFriendshipMultiplier 4 (the non-event value is 3), luckBoost 1,
 * trainerShinyChance 6554 (1 in 10), upgradeUnlockedVouchers true. */
#include "cc/tevents.h"

const cc_tevent cc_tevents[] = {
    /* daily, all day: the mild perks */
    {"lucky_catch", "Lucky Catch", {1, 0, 0, 1440}, {.shiny_catch_mult = 3}},          /* shiny targets are caught x3 instead of x2 */
    {"sweet_tooth", "Sweet Tooth", {1, 0, 0, 1440}, {.candy_mult = 4}},          /* Classic candy friendship x4 instead of x3 */
    /* every two or three days, a whole day on (lorenzo, 2026-11-16: Shiny Day every 3 days, Voucher Plus every 2): the medium perks */
    {"shiny_day", "Shiny Day", {3, 0, 0, 1440}, {.shiny_encounter_mult = 2}},              /* every third day: wild shiny odds x2 */
    {"voucher_plus_day", "Voucher Plus Day", {2, 1, 0, 1440}, {.upgrade_vouchers = 1}}, /* every second day: defeated bosses give the upgraded voucher */
    /* one hour each day: the generous perks */
    {"shiny_trainers", "Shiny Trainers", {1, 0, 780, 60}, {.trainer_shiny_chance = 6554}},   /* 13:00 to 14:00: a trainer's Pokemon are shiny 1 in 10 */
    {"golden_hour", "Golden Hour", {1, 0, 1200, 60}, {.luck_boost = 1}},          /* 20:00 to 21:00: party luck +1 */

    /* ---- second round (lorenzo, 2026-11-16: "after you're done with the first 6 move onto the other 8"). DRAFT like the rest: the hours and
     * days are mine and he has not seen them; the items and species are upstream's own kinds of effect with my choice of contents. ---- */
    /* one hour each day */
    {"charm_hour", "Charm Hour", {1, 0, 1080, 60},                                  /* 18:00 to 19:00: the charms of upstream's Winter Holiday event */
     {.rewards = {{8, CC_TITEM_SHINY_CHARM}, {8, CC_TITEM_ABILITY_CHARM}, {8, CC_TITEM_CATCHING_CHARM}, {25, CC_TITEM_SHINY_CHARM}}}},
    {"lucky_species_hour", "Lucky Species Hour", {1, 0, 960, 60},                   /* 16:00 to 17:00: each Eevee line member in the party is +1 luck */
     {.luck_species = {133, 134, 135, 136, 196, 197, 470, 471, 700}}},
    /* a whole day, every two or three days */
    {"candy_jar_day", "Candy Jar Day", {2, 0, 0, 1440}, {.rewards = {{25, CC_TITEM_CANDY_JAR}}}},   /* a Candy Jar after wave 25 */
    {"delibirdy_day", "Delibirdy Day", {3, 1, 0, 1440},                             /* Delibird-y: common, and a charm on top */
     {.delibirdy_buff = {CC_TITEM_CATCHING_CHARM, CC_TITEM_SHINY_CHARM, CC_TITEM_ABILITY_CHARM, CC_TITEM_EXP_CHARM, CC_TITEM_SUPER_EXP_CHARM,
                         CC_TITEM_HEALING_CHARM},
      .flags = CC_TFX_DELIBIRDY_COMMON}},
    {"fusion_day", "Fusion Day", {3, 2, 0, 1440}, {.flags = CC_TFX_BOOST_FUSIONS}},   /* fusions feed their candy 1/1.5 each */
    /* daily, all day */
    {"waiting_room", "Waiting Room", {1, 0, 0, 1440},                               /* the Mega Bracelet and Dynamax Band, after wave 115 */
     {.rewards = {{115, CC_TITEM_MEGA_BRACELET}, {115, CC_TITEM_DYNAMAX_BAND}}}},
    {"snowy_town", "Snowy Town", {1, 0, 0, 1440}, {.flags = CC_TFX_TOWN_SNOW}},     /* the Town biome is always snowing */
};
const unsigned cc_tevent_count = sizeof cc_tevents / sizeof cc_tevents[0];
