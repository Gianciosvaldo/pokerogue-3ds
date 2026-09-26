#include "save.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC 0x31535250u /* "PRS1" little-endian */

typedef struct {
  uint32_t magic;
  uint32_t len;
  uint32_t crc;
} header;

static uint32_t crc32(const void* data, size_t n) {
  const uint8_t* p = data;
  uint32_t c = 0xFFFFFFFFu;
  while (n--) {
    c ^= *p++;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1));
  }
  return ~c;
}

static void path_of(char* buf, size_t n, const char* name, const char* ext) {
  snprintf(buf, n, "%s/%s%s", SAVE_DIR, name, ext);
}

bool save_init(void) {
  mkdir("sdmc:/3ds", 0777);
  mkdir("sdmc:/3ds/pokerogue", 0777);
  if (mkdir(SAVE_DIR, 0777) != 0 && errno != EEXIST) {
    struct stat st;
    return stat(SAVE_DIR, &st) == 0;
  }
  return true;
}

static save_result read_one(const char* path, char** out, size_t* len) {
  FILE* f = fopen(path, "rb");
  if (!f) return SAVE_NOT_FOUND;
  header h;
  if (fread(&h, sizeof h, 1, f) != 1 || h.magic != MAGIC || h.len > 64u * 1024 * 1024) {
    fclose(f);
    return SAVE_CORRUPT;
  }
  char* buf = malloc(h.len + 1);
  if (!buf) {
    fclose(f);
    return SAVE_IO_ERROR;
  }
  if ((h.len && fread(buf, h.len, 1, f) != 1) || crc32(buf, h.len) != h.crc) {
    fclose(f);
    free(buf);
    return SAVE_CORRUPT;
  }
  fclose(f);
  buf[h.len] = 0;
  *out = buf;
  *len = h.len;
  return SAVE_OK;
}

save_result save_write(const char* name, const void* data, size_t len) {
  char main_p[256], tmp_p[256], bak_p[256];
  path_of(main_p, sizeof main_p, name, ".sav");
  path_of(tmp_p, sizeof tmp_p, name, ".sav.tmp");
  path_of(bak_p, sizeof bak_p, name, ".sav.bak");

  FILE* f = fopen(tmp_p, "wb");
  if (!f) return SAVE_IO_ERROR;
  header h = {MAGIC, (uint32_t)len, crc32(data, len)};
  bool ok = fwrite(&h, sizeof h, 1, f) == 1 && (len == 0 || fwrite(data, len, 1, f) == 1);
  ok = (fflush(f) == 0) && ok;
  ok = (fclose(f) == 0) && ok;
  if (!ok) {
    remove(tmp_p);
    return SAVE_IO_ERROR;
  }
  /* The 3DS SD archive refuses to rename onto an existing file, so rotate by hand.
   * A main file that fails its CRC is dropped instead of replacing a good .bak. */
  char* old;
  size_t old_len;
  save_result cur = read_one(main_p, &old, &old_len);
  if (cur == SAVE_OK) {
    free(old);
    remove(bak_p);
    if (rename(main_p, bak_p) != 0) return SAVE_IO_ERROR;
  } else if (cur == SAVE_CORRUPT) {
    remove(main_p);
  }
  if (rename(tmp_p, main_p) != 0) return SAVE_IO_ERROR;
  return SAVE_OK;
}

save_result save_read(const char* name, char** out, size_t* len, bool* used_backup) {
  char main_p[256], bak_p[256];
  path_of(main_p, sizeof main_p, name, ".sav");
  path_of(bak_p, sizeof bak_p, name, ".sav.bak");
  if (used_backup) *used_backup = false;
  save_result r = read_one(main_p, out, len);
  if (r == SAVE_OK) return r;
  save_result rb = read_one(bak_p, out, len);
  if (rb == SAVE_OK) {
    if (used_backup) *used_backup = true;
    return SAVE_OK;
  }
  if (r == SAVE_NOT_FOUND && rb == SAVE_NOT_FOUND) return SAVE_NOT_FOUND;
  return SAVE_CORRUPT;
}

const char* save_result_str(save_result r) {
  switch (r) {
    case SAVE_OK: return "ok";
    case SAVE_NOT_FOUND: return "not found";
    case SAVE_CORRUPT: return "corrupt";
    default: return "I/O error";
  }
}
