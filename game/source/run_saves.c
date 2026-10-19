/* The run's save slots (see run_saves.h). */
#include "run_saves.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "save.h"

#define LAST_SLOT_FILE "last_slot"

/* Not on the stack (GC_SAVE_MAX is 128 KB, the 3DS stack 32 KB). One buffer for writing, and one run to load a slot into when
 * all that is wanted is what it holds. */
static uint8_t g_wbuf[GC_SAVE_MAX];
static gc_run g_scratch;

bool rsv_slot_ok(int slot) { return slot >= 0 && slot < RSV_SLOTS; }

const char* rsv_slot_name(int slot, char out[16]) {
  snprintf(out, 16, "session_%d", slot);
  return out;
}

/* FNV-1a over the run seed's UTF-16 units: the key a run's name is kept under */
static uint64_t run_key_of(const gc_run* run) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < run->run.seed.len && i < sizeof run->run.seed.u / sizeof run->run.seed.u[0]; i++) {
    h ^= run->run.seed.u[i] & 0xffu;
    h *= 1099511628211ull;
    h ^= run->run.seed.u[i] >> 8;
    h *= 1099511628211ull;
  }
  return h;
}

void rsv_summarize(const gc_run* run, rsv_info* out) {
  out->run_key = run_key_of(run);
  out->mode = run->run.mode;
  out->wave = run->wave;
  out->biome = run->biome;
  out->score = run->score;
  out->money = run->money;
  out->n_party = run->n_party < 6 ? run->n_party : 6;
  for (int i = 0; i < out->n_party; i++) {
    int idx = run->party_order[i];
    if (idx < 0 || idx >= run->n_party) idx = i;
    const gc_mon* m = &run->party[idx];
    rsv_mon* o = &out->party[i];
    o->species = m->b.species;
    o->form = (uint8_t)m->b.form;
    o->gender = (int8_t)m->b.gender;
    o->shiny = m->b.shiny;
    o->variant = m->variant;
    o->level = m->b.level;
    o->fainted = m->b.hp <= 0;
  }
}

/* game-core's reason for refusing a save, in the words the slot list shows */
static void refusal(const char* why, char out[RSV_WHY_LEN]) {
  if (!why) why = "";
  if (!strcmp(why, "the save was made by a newer version of the game"))
    snprintf(out, RSV_WHY_LEN, "Made by a newer version of the game.");
  else if (!strcmp(why, "the save is damaged"))
    snprintf(out, RSV_WHY_LEN, "The save is damaged.");
  else if (!strcmp(why, "not a run save"))
    snprintf(out, RSV_WHY_LEN, "Not a run save.");
  else
    snprintf(out, RSV_WHY_LEN, "Can't be read: %.60s.", why);
}

/* Loads core bytes into `run`. `info` (may be NULL) gets the status and the reason. */
rsv_status rsv_load_bytes(const char* buf, size_t len, gc_run* run, rsv_info* info) {
  char why[RSV_WHY_LEN] = "";
  rsv_status st;
  const char* gc_why = NULL;
  if (gc_run_load(run, buf, len, &gc_why)) {
    st = RSV_OK;
  } else {
    st = RSV_INCOMPATIBLE;
    refusal(gc_why, why);
  }
  if (info) {
    memset(info, 0, sizeof *info);
    info->status = st;
    snprintf(info->why, sizeof info->why, "%s", why);
    if (st == RSV_OK) rsv_summarize(run, info);
  }
  return st;
}

rsv_status rsv_inspect_bytes(const char* buf, size_t len, rsv_info* info) { return rsv_load_bytes(buf, len, &g_scratch, info); }

/* Reads the slot's file and loads it into `run`. `info` (may be NULL) gets the status and the reason. */
static rsv_status read_into(int slot, gc_run* run, rsv_info* info) {
  char name[16];
  char* buf = NULL;
  size_t len = 0;
  bool bak = false;
  rsv_status st;
  char why[RSV_WHY_LEN] = "";
  if (!rsv_slot_ok(slot)) {
    st = RSV_EMPTY;
  } else {
    save_result r = save_read(rsv_slot_name(slot, name), &buf, &len, &bak);
    if (r == SAVE_NOT_FOUND) {
      st = RSV_EMPTY;
    } else if (r == SAVE_CORRUPT) {
      st = RSV_INCOMPATIBLE;
      snprintf(why, sizeof why, "The save is damaged (it failed its checksum).");
    } else if (r != SAVE_OK) {
      st = RSV_INCOMPATIBLE;
      snprintf(why, sizeof why, "The save could not be read from the SD card.");
    } else {
      rsv_info loaded;
      st = rsv_load_bytes(buf, len, run, &loaded);
      snprintf(why, sizeof why, "%s", loaded.why);
      free(buf);
      if (info && st == RSV_OK) {
        *info = loaded;
        info->from_backup = bak;
        return st;
      }
    }
  }
  if (info) {
    memset(info, 0, sizeof *info);
    info->status = st;
    snprintf(info->why, sizeof info->why, "%s", why);
  }
  return st;
}

rsv_status rsv_inspect(int slot, rsv_info* out) { return read_into(slot, &g_scratch, out); }

void rsv_inspect_all(rsv_info out[RSV_SLOTS]) {
  for (int i = 0; i < RSV_SLOTS; i++) rsv_inspect(i, &out[i]);
}

rsv_status rsv_load(int slot, gc_run* run, char why[RSV_WHY_LEN]) {
  rsv_info info;
  rsv_status st = read_into(slot, run, &info);
  if (why) snprintf(why, RSV_WHY_LEN, "%s", info.why);
  if (st == RSV_OK) rsv_set_last_slot(slot);
  return st;
}

rsv_save_result rsv_save(int slot, const gc_run* run) {
  char name[16];
  if (!rsv_slot_ok(slot)) return RSV_BAD_SLOT;
  if (!gc_run_can_save(run)) return RSV_NOT_ALLOWED;
  size_t n = gc_run_save(run, g_wbuf, sizeof g_wbuf);
  if (!n) return RSV_NOT_ALLOWED;
  if (save_write(rsv_slot_name(slot, name), g_wbuf, n) != SAVE_OK) return RSV_IO_ERROR;
  rsv_set_last_slot(slot);
  return RSV_SAVED;
}

/* Removes `name`.sav, `name`.sav.bak and `name`.sav.tmp (save.c's files); false when one that exists would not go. */
bool rsv_remove_files(const char* name) {
  static const char* const ext[] = {".sav", ".sav.bak", ".sav.tmp"};
  char path[256];
  bool ok = true;
  for (int i = 0; i < 3; i++) {
    snprintf(path, sizeof path, "%s/%s%s", SAVE_DIR, name, ext[i]);
    FILE* f = fopen(path, "rb");
    if (!f) continue; /* not there */
    fclose(f);
    if (remove(path) != 0) ok = false;
  }
  return ok;
}

bool rsv_clear(int slot) {
  if (!rsv_slot_ok(slot)) return false;
  char name[16];
  const bool was_last = rsv_last_slot() == slot;
  if (!rsv_remove_files(rsv_slot_name(slot, name))) return false;
  rsv_name_remove(name); /* the name goes with the run; one that is left behind is never shown for another run (rsv_name_get) */
  /* upstream's deleteSession: the slot is no longer the one Continue loads */
  if (was_last) rsv_remove_files(LAST_SLOT_FILE);
  return true;
}

int rsv_last_slot(void) {
  char* buf = NULL;
  size_t len = 0;
  int slot = -1;
  if (save_read(LAST_SLOT_FILE, &buf, &len, NULL) == SAVE_OK) {
    if (len == 1 && rsv_slot_ok((uint8_t)buf[0])) slot = (uint8_t)buf[0];
    free(buf);
  }
  return slot;
}

void rsv_set_last_slot(int slot) {
  if (!rsv_slot_ok(slot) || rsv_last_slot() == slot) return;
  const char b = (char)slot;
  save_write(LAST_SLOT_FILE, &b, 1);
}

/* ------------------------------------------------------------------ run names */

#define NAME_MAGIC "RN1"
#define NAME_HEAD 11 /* the magic and the 8-byte run key */

int rsv_utf8_chars(const char* s) {
  int n = 0;
  const unsigned char* p = (const unsigned char*)s;
  while (*p) {
    int len;
    if (*p < 0x80)
      len = 1;
    else if ((*p & 0xe0) == 0xc0 && *p >= 0xc2)
      len = 2;
    else if ((*p & 0xf0) == 0xe0)
      len = 3;
    else if ((*p & 0xf8) == 0xf0 && *p <= 0xf4)
      len = 4;
    else
      return -1;
    for (int i = 1; i < len; i++)
      if ((p[i] & 0xc0) != 0x80) return -1; /* also ends at the NUL */
    p += len;
    n++;
  }
  return n;
}

/* the name's file: "<slot_file>_name" (+ .sav and friends) */
static const char* name_file(const char* slot_file, char out[40]) {
  snprintf(out, 40, "%.30s_name", slot_file);
  return out;
}

static bool name_ok(const char* s) {
  const int n = rsv_utf8_chars(s);
  if (n < 1 || n > RSV_NAME_MAX) return false;
  for (const unsigned char* p = (const unsigned char*)s; *p; p++)
    if (*p < 0x20 || *p == 0x7f) return false;
  return true;
}

bool rsv_name_get(const char* slot_file, uint64_t run_key, char out[RSV_NAME_LEN]) {
  out[0] = '\0';
  char fn[40];
  char* buf = NULL;
  size_t len = 0;
  if (save_read(name_file(slot_file, fn), &buf, &len, NULL) != SAVE_OK) return false;
  bool ok = false;
  if (len > NAME_HEAD && len - NAME_HEAD < RSV_NAME_LEN && !memcmp(buf, NAME_MAGIC, 3)) {
    uint64_t key = 0;
    for (int i = 0; i < 8; i++) key |= (uint64_t)(uint8_t)buf[3 + i] << (8 * i);
    char text[RSV_NAME_LEN];
    memcpy(text, buf + NAME_HEAD, len - NAME_HEAD);
    text[len - NAME_HEAD] = '\0';
    if (key == run_key && name_ok(text)) {
      memcpy(out, text, len - NAME_HEAD + 1);
      ok = true;
    }
  }
  free(buf);
  return ok;
}

bool rsv_name_set(const char* slot_file, uint64_t run_key, const char* name) {
  if (!name || !name_ok(name)) return false;
  char fn[40];
  uint8_t buf[NAME_HEAD + RSV_NAME_LEN];
  memcpy(buf, NAME_MAGIC, 3);
  for (int i = 0; i < 8; i++) buf[3 + i] = (uint8_t)(run_key >> (8 * i));
  const size_t n = strlen(name);
  memcpy(buf + NAME_HEAD, name, n);
  return save_write(name_file(slot_file, fn), buf, NAME_HEAD + n) == SAVE_OK;
}

bool rsv_name_remove(const char* slot_file) {
  char fn[40];
  return rsv_remove_files(name_file(slot_file, fn));
}
