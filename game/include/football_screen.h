#ifndef GM_FOOTBALL_SCREEN_H
#define GM_FOOTBALL_SCREEN_H
#include <stdbool.h>
#include <stdint.h>
#include "gc/system.h"
typedef struct {
    gc_system *profile;
    uint32_t entropy;
    void (*on_exit)(void *arg);
    void *arg;
    bool (*save_profile)(void *arg); /* Optional; app uses the existing profile save gate. */
    const char *profile_notice; /* Optional startup-only notice for an unreadable profile. */
} football_screen_args;
/* Reuses the starter selector, then replaces it with the football screen.
 * The playoff is session-only; no classic/custom save slot is touched. */
bool football_screen_open(const football_screen_args *args);
#ifdef FOOTBALL_SCREEN_TEST
#include "football_mode.h"
const FootballMode *football_screen_test_mode(void);
#endif
#endif
