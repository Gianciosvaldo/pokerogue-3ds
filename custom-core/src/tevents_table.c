/* custom-core: the timed events (data; see cc/tevents.h).
 *
 * DRAFT. lorenzo (2026-11-16) chose his own events instead of upstream's, with three tiers by how giving an event is: daily (all day),
 * every two or three days, one hour each day. He asked for Shiny Day every 3 days and Voucher Plus every 2, and for the events to take
 * their effects from upstream's. The six rows below are the thread's proposal for him to cut and change; he has not approved the whole
 * list, and NO code reads their effects yet (this PR adds the schedule model only). Times are the console's local time.
 *
 * Numbers come from upstream's own events at the pin 8555c08 (src/data/balance/timed-events.ts): shinyEncounterMultiplier 2,
 * shinyCatchMultiplier 3 (the non-event value is 2), classicFriendshipMultiplier 4 (the non-event value is 3), luckBoost 1,
 * trainerShinyChance 6554 (1 in 10), upgradeUnlockedVouchers true. */
#include "cc/tevents.h"

const cc_tevent cc_tevents[] = {
    /* daily, all day: the mild perks */
    {"lucky_catch", "Lucky Catch", {1, 0, 0, 1440}, {0, 3, 0, 0, 0, 0}},          /* shiny targets are caught x3 instead of x2 */
    {"sweet_tooth", "Sweet Tooth", {1, 0, 0, 1440}, {0, 0, 4, 0, 0, 0}},          /* Classic candy friendship x4 instead of x3 */
    /* every two or three days, a whole day on (lorenzo, 2026-11-16: Shiny Day every 3 days, Voucher Plus every 2): the medium perks */
    {"shiny_day", "Shiny Day", {3, 0, 0, 1440}, {2, 0, 0, 0, 0, 0}},              /* every third day: wild shiny odds x2 */
    {"voucher_plus_day", "Voucher Plus Day", {2, 1, 0, 1440}, {0, 0, 0, 0, 0, 1}}, /* every second day: defeated bosses give the upgraded voucher */
    /* one hour each day: the generous perks */
    {"shiny_trainers", "Shiny Trainers", {1, 0, 780, 60}, {0, 0, 0, 0, 6554, 0}},   /* 13:00 to 14:00: a trainer's Pokemon are shiny 1 in 10 */
    {"golden_hour", "Golden Hour", {1, 0, 1200, 60}, {0, 0, 0, 1, 0, 0}},          /* 20:00 to 21:00: party luck +1 */
};
const unsigned cc_tevent_count = sizeof cc_tevents / sizeof cc_tevents[0];
