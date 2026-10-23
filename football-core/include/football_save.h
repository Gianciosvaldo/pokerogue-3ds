#ifndef FOOTBALL_SAVE_H
#define FOOTBALL_SAVE_H
#include "football_mode.h"
#define FB_SAVE_MAX (GC_SAVE_MAX + 4096u)
bool football_save_ready(const FootballMode *mode);
size_t football_save_encode(const FootballMode *mode, void *buffer, size_t capacity);
/* Load into fresh caller-owned mode/run; failure leaves these unusable. No live
 * state or shared profile is changed. File wrapper loads only at mode entry. */
bool football_save_decode(FootballMode *mode, gc_run *roster, gc_system *profile,
                           const void *buffer, size_t length);
#endif
