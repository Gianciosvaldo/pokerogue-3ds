/* RPAK reader. See include/pak.h and assets-pipeline/rpak.py. */
#include "pak.h"

#include <lz4.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MOUNTS 8

typedef struct {
  uint64_t hash;
  uint32_t offset, stored, size;
  uint8_t codec, type;
  uint16_t pad;
} __attribute__((packed)) pak_rec;

struct rr_pak {
  FILE* f;
  uint32_t names_off, names_size;
  char path[128];
  uint32_t count;
  pak_rec* index;
};

static rr_pak* g_mounts[MAX_MOUNTS];
static int g_nmounts;

uint64_t pak_hash(const char* key) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (const unsigned char* s = (const unsigned char*)key; *s; s++) h = (h ^ *s) * 0x100000001B3ull;
  return h;
}

rr_pak* pak_open(const char* path) {
  FILE* f = fopen(path, "rb");
  if (!f) return NULL;
  uint32_t hdr[8];
  if (fread(hdr, 4, 8, f) != 8 || memcmp(hdr, "RPK1", 4) || hdr[1] != 1) {
    fclose(f);
    return NULL;
  }
  rr_pak* p = calloc(1, sizeof *p);
  p->f = f;
  p->count = hdr[2];
  p->names_off = hdr[4];
  p->names_size = hdr[5];
  snprintf(p->path, sizeof p->path, "%s", path);
  p->index = malloc(sizeof(pak_rec) * p->count);
  if (!p->index || fseek(f, hdr[3], SEEK_SET) || fread(p->index, sizeof(pak_rec), p->count, f) != p->count) {
    pak_close(p);
    return NULL;
  }
  return p;
}

void pak_close(rr_pak* p) {
  if (!p) return;
  if (p->f) fclose(p->f);
  free(p->index);
  free(p);
}

int pak_count(const rr_pak* p) { return p ? (int)p->count : 0; }
const char* pak_path(const rr_pak* p) { return p ? p->path : ""; }

int pak_mount_default(void) {
  static const char* names[] = {"gfx", "snd", "bgm", "font", "data"};
  for (int i = 0; i < 5 && g_nmounts < MAX_MOUNTS; i++) {
    char path[128];
    snprintf(path, sizeof path, PAK_DIR "%s.pak", names[i]);
    rr_pak* p = pak_open(path);
    if (p) g_mounts[g_nmounts++] = p;
  }
  return g_nmounts;
}

void pak_unmount_all(void) {
  for (int i = 0; i < g_nmounts; i++) pak_close(g_mounts[i]);
  g_nmounts = 0;
}

static void fill(rr_pak* p, const pak_rec* r, pak_entry* out) {
  out->pak = p;
  out->offset = r->offset;
  out->stored = r->stored;
  out->size = r->size;
  out->codec = r->codec;
  out->type = r->type;
}

static bool find_one(rr_pak* p, uint64_t h, pak_entry* out) {
  uint32_t lo = 0, hi = p->count;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    if (p->index[mid].hash < h)
      lo = mid + 1;
    else
      hi = mid;
  }
  if (lo < p->count && p->index[lo].hash == h) {
    fill(p, &p->index[lo], out);
    return true;
  }
  return false;
}

bool pak_find(rr_pak* p, const char* key, pak_entry* out) {
  uint64_t h = pak_hash(key);
  if (p) return find_one(p, h, out);
  for (int i = 0; i < g_nmounts; i++)
    if (find_one(g_mounts[i], h, out)) return true;
  return false;
}

bool pak_entry_at(rr_pak* p, int i, pak_entry* out) {
  if (!p || i < 0 || (uint32_t)i >= p->count) return false;
  fill(p, &p->index[i], out);
  return true;
}

void* pak_read_entry(const pak_entry* e, size_t* len) {
  if (!e || !e->pak) return NULL;
  char* out = malloc(e->size + 1);
  if (!out) return NULL;
  FILE* f = e->pak->f;
  bool ok = false;
  if (fseek(f, e->offset, SEEK_SET) == 0) {
    if (e->codec == 0) {
      ok = fread(out, 1, e->size, f) == e->size;
    } else if (e->codec == 1) {
      char* tmp = malloc(e->stored);
      if (tmp && fread(tmp, 1, e->stored, f) == e->stored)
        ok = LZ4_decompress_safe(tmp, out, (int)e->stored, (int)e->size) == (int)e->size;
      free(tmp);
    }
  }
  if (!ok) {
    free(out);
    return NULL;
  }
  out[e->size] = 0;
  if (len) *len = e->size;
  return out;
}

void* pak_read(const char* key, size_t* len) {
  pak_entry e;
  return pak_find(NULL, key, &e) ? pak_read_entry(&e, len) : NULL;
}

FILE* pak_fopen(const rr_pak* p) { return p ? fopen(p->path, "rb") : NULL; }

char* pak_load_names(const rr_pak* p, size_t* len) {
  if (!p) return NULL;
  char* b = malloc(p->names_size + 1);
  if (!b) return NULL;
  if (fseek(p->f, p->names_off, SEEK_SET) || fread(b, 1, p->names_size, p->f) != p->names_size) {
    free(b);
    return NULL;
  }
  b[p->names_size] = 0;
  if (len) *len = p->names_size;
  return b;
}
