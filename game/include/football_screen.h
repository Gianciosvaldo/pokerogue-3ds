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
    bool persistence; /* App enables the dedicated football checkpoint file. */
    const char *profile_notice; /* Optional startup-only notice for an unreadable profile. */
} football_screen_args;
/* Live 22-player football, with its own 40-point draft and lineup screen.
 * Optional dedicated football checkpoints; no classic/custom save slot is touched. */
bool football_screen_open(const football_screen_args *args);
/* Explicit compatibility path for original six-player playoff saves. */
bool football_legacy_screen_open(const football_screen_args *args);
#ifdef FOOTBALL_SCREEN_TEST
#include "football_mode.h"
const FootballMode *football_screen_test_mode(void);
#endif
#endif
