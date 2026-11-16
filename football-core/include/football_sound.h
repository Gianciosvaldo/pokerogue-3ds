#ifndef FOOTBALL_SOUND_H
#define FOOTBALL_SOUND_H
#include "football_live.h"
/* The sounds of a live match. The engine stays silent and unchanged: a watcher looks at the match once per frame, compares it with
 * what it saw the frame before, and lists the sounds that frame earned (an impact for each new fx record, a snap, a throw or a kick,
 * a first down, a score, the end of a quarter, the final whistle). The screen plays the list. The watcher keeps its own state and
 * never writes to the match, so it cannot change a result, and the same match gives the same sounds at any frame rate.
 *
 * The human always plays HOME: a score, a turnover or a first down is told "for" the human when HOME gains by it. */
typedef enum {
    FB_SND_NONE,
    FB_SND_SNAP,FB_SND_THROW,FB_SND_KICK,FB_SND_CATCH,FB_SND_INCOMPLETE,FB_SND_JUKE,FB_SND_DIVE,FB_SND_TACKLE,FB_SND_SACK,
    FB_SND_FUMBLE,FB_SND_INTERCEPTION,FB_SND_BLOCKED,FB_SND_MUFF,
    FB_SND_TOUCHDOWN_FOR,FB_SND_TOUCHDOWN_AGAINST,FB_SND_SCORE_FOR,FB_SND_SCORE_AGAINST,FB_SND_NO_GOOD,
    FB_SND_FIRST_DOWN,FB_SND_TURNOVER_GAIN,FB_SND_TURNOVER_LOSS,FB_SND_PERIOD_END,FB_SND_WIN,FB_SND_LOSS,
    FB_SND_COUNT
} FootballSound;
/* A frame is at most 100 ms of the match, so a handful of sounds is all it can hold. */
#define FB_SOUND_MAX 12
typedef struct {
    bool seen;                 /* a first look has been taken (football_sound_reset) */
    bool flying,ended;
    uint8_t phase,offense,quarter;
    uint32_t snaps;
    uint16_t score[2];
    uint8_t fx_kind[FB_FX_COUNT];uint32_t fx_step[FB_FX_COUNT];
} FootballSoundWatch;
/* Takes the first look without making a sound: call it when a match begins, or when the watcher starts looking at a match already going. */
void football_sound_reset(FootballSoundWatch *w,const FootballLive *m);
/* Compares the match with the last look, remembers it, and writes the sounds in the order they happened. Returns how many (0..FB_SOUND_MAX). */
unsigned football_sound_scan(FootballSoundWatch *w,const FootballLive *m,FootballSound out[FB_SOUND_MAX]);
/* The sound pack's key for a sound (upstream's "se/hit", "ui/select", ...), NULL for FB_SND_NONE. */
const char *football_sound_key(FootballSound sound);
#endif
