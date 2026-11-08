/*
 * img_cache: the rules of the menus' image cache (mui_frame), apart from the renderer, so they can be tested on the host.
 *
 * The menus ask for atlases by key ("ui/party_bg", "pokemon_icons_4", "items") and keep them for the whole session: a lookup is a hit, a
 * load or a remembered absence. The 3DS has 32 MB of linear memory for every texture, so a load can fail for lack of room, which is not
 * the same as the pack not having the key:
 *
 *  - a key the pack does not hold is remembered as missing, for good;
 *  - a key the pack holds that did not load (memory) is NOT remembered as missing: the cache frees atlases nobody drew this frame, the
 *    biggest being the Pokemon icon sheets (pokemon_icons_*, which reload from the pack the next time a screen draws one), and tries
 *    again; when that does not help either, the lookup says "not there" now and tries again RETRY_FRAMES frames later.
 *
 * Before this, one failed load was remembered as missing until the whole cache was dropped, so a sheet of icons that failed once (the
 * starter select scrolling into a generation while the last run's battle graphics were still loaded) stayed blank at random.
 *
 * A frame pointer taken from an atlas stays valid for the frame it was taken in: only atlases not drawn in the current frame are ever
 * freed to make room. `imgc_tick` is called once per frame (mui_tick).
 */
#ifndef GM_IMG_CACHE_H
#define GM_IMG_CACHE_H

#include <stdbool.h>
#include <stddef.h>

#include "rr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMGC_SLOTS 96
#define IMGC_RETRY_FRAMES 30   /* a load that failed for lack of room is tried again after this many frames (half a second) */

typedef struct {
  rr_atlas* (*load)(const char* key);  /* the atlas of pack key `key`, or NULL */
  void (*unload)(rr_atlas* atlas);
  bool (*exists)(const char* key);     /* the pack holds `key` (so a NULL from load() is a failed load, not an absence) */
} imgc_ops;

void imgc_init(const imgc_ops* ops);

/* The atlas for `key` (the menus' image keys, without the "images/" the pack adds), or NULL. */
rr_atlas* imgc_get(const char* key);

/* Once per frame: atlases drawn in the frame that began are safe from eviction until the next call. */
void imgc_tick(void);

/* Frees every atlas that was not drawn this frame and is evictable (an icon sheet); returns how many. For a caller about to load
 * something big (the starter select's Pokemon picture) that failed. */
int imgc_reclaim(void);

/* Frees everything (mui_unload_all). */
void imgc_clear(void);

/* How many loads failed for lack of room since imgc_init, and the key of the last. NULL when none. */
const char* imgc_last_failure(unsigned* count);

#ifdef __cplusplus
}
#endif
#endif
