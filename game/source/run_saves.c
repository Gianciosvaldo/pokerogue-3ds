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

void rsv_summarize(const gc_run* run, rsv_info* out) {
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
    o->level = m->b.level > 255 ? 255 : (uint8_t)m->b.level;
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
