/*
 * pak: reads the RPAK archives the asset pipeline writes to the SD card
 * (sdmc:/3ds/pokerogue/{gfx,snd,bgm,font,data}.pak). See assets-pipeline/rpak.py.
 *
 * Keys are upstream paths: "images/pokemon/back/642-therian" (no extension) for
 * images, "audio/bgm/title" for audio, "battle-anims/absorb.json" for data.
 * The index (24 bytes per entry) is held in RAM; entry names are not.
 */
#ifndef PAK_H
#define PAK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAK_DIR "sdmc:/3ds/pokerogue/"

enum { PAK_TEX = 1, PAK_SND = 2, PAK_JSON = 3, PAK_BIN = 4, PAK_FONT = 5, PAK_VARIANT = 6 };

typedef struct rr_pak rr_pak;

typedef struct {
  rr_pak* pak;
  uint32_t offset, stored, size;
  uint8_t codec, type;
} pak_entry;

uint64_t pak_hash(const char* key);

rr_pak* pak_open(const char* path);
void pak_close(rr_pak* p);
int pak_count(const rr_pak* p);
const char* pak_path(const rr_pak* p);

/* Opens PAK_DIR{gfx,snd,bgm,font,data}.pak, whichever exist. Returns how many opened. */
int pak_mount_default(void);
void pak_unmount_all(void);

/* Looks a key up in one pak, or in every mounted pak when p is NULL. */
bool pak_find(rr_pak* p, const char* key, pak_entry* out);
/* Same, by index (0..count-1, hash order); for tests that walk a whole pak. */
bool pak_entry_at(rr_pak* p, int i, pak_entry* out);

/* Reads and decompresses an entry. Returns a malloc'd buffer (NUL-terminated
 * one byte past *len, handy for JSON), or NULL. */
void* pak_read_entry(const pak_entry* e, size_t* len);
void* pak_read(const char* key, size_t* len);

/* Raw reads of an uncompressed entry, for streaming (BGM). Each caller that
 * streams from another thread must use its own FILE*: pak_fopen gives one. */
FILE* pak_fopen(const rr_pak* p);

/* All entry names, NUL-separated in index order (malloc'd; tests/debug only). */
char* pak_load_names(const rr_pak* p, size_t* len);

#ifdef __cplusplus
}
#endif
#endif
