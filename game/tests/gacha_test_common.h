/* gacha_test_common.h: what gacha_host_test.c and gacha_screen_test.c share (task C09): the fault-injecting SD card under the profile, the
 * custom file and the purchase record; snapshots of the save folder; the app's clock and entropy hooks; the profile digests; "reboot"
 * (rebuild everything from the files); a fresh card; and game-core's own answer for the same inputs. The includes, `checks`/`failed` and
 * CHECK() come from the test that includes this. */
#ifndef GACHA_TEST_COMMON_H
#define GACHA_TEST_COMMON_H
#define UNUSED __attribute__((unused))
/* ------------------------------------------------------------------ the fault-injecting card */

static int g_w;            /* writes attempted since it was reset */
static int g_fail_at;      /* that write fails, nothing written (0: none) */
static int g_cut_before;   /* the power is cut before that write: it and every later I/O never happen (0: none) */
static bool g_dead;

UNUSED static save_result f_read(const char* name, char** out, size_t* len, bool* used_backup) {
  if (g_dead) return SAVE_IO_ERROR;
  return save_read(name, out, len, used_backup);
}
UNUSED static save_result f_write(const char* name, const void* data, size_t len) {
  g_w++;
  if (g_dead) return SAVE_IO_ERROR;
  if (g_cut_before && g_w == g_cut_before) {
    g_dead = true;
    return SAVE_IO_ERROR;
  }
  if (g_fail_at && g_w == g_fail_at) return SAVE_IO_ERROR;
  return save_write(name, data, len);
}
UNUSED static void faults(int fail_at, int cut_before) {
  g_w = 0;
  g_fail_at = fail_at;
  g_cut_before = cut_before;
  g_dead = false;
}
static void no_faults(void) { faults(0, 0); }

/* ------------------------------------------------------------------ files */

typedef struct {
  char name[256];
  uint8_t* bytes;
  size_t len;
} snap_file;
typedef struct {
  snap_file f[24];
  int n;
} snap;

UNUSED static snap take(void) {
  snap s;
  memset(&s, 0, sizeof s);
  DIR* d = opendir(SAVE_DIR);
  struct dirent* e;
  while (d && (e = readdir(d)) && s.n < 24) {
    if (e->d_name[0] == '.') continue;
    snap_file* f = &s.f[s.n++];
    snprintf(f->name, sizeof f->name, "%s", e->d_name);
    char path[600];
    snprintf(path, sizeof path, "%s/%s", SAVE_DIR, e->d_name);
    FILE* fp = fopen(path, "rb");
    fseek(fp, 0, SEEK_END);
    f->len = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    f->bytes = malloc(f->len + 1);
    if (fread(f->bytes, 1, f->len, fp) != f->len) f->len = 0;
    fclose(fp);
  }
  if (d) closedir(d);
  return s;
}
UNUSED static void drop(snap* s) {
  for (int i = 0; i < s->n; i++) free(s->f[i].bytes);
  s->n = 0;
}
UNUSED static void wipe(void) {
  DIR* d = opendir(SAVE_DIR);
  struct dirent* e;
  while (d && (e = readdir(d))) {
    if (e->d_name[0] == '.') continue;
    char path[600];
    snprintf(path, sizeof path, "%s/%s", SAVE_DIR, e->d_name);
    remove(path);
  }
  if (d) closedir(d);
}
UNUSED static bool same_files(const snap* a, const snap* b) {
  if (a->n != b->n) return false;
  for (int i = 0; i < a->n; i++) {
    bool found = false;
    for (int j = 0; j < b->n; j++)
      if (!strcmp(a->f[i].name, b->f[j].name)) found = a->f[i].len == b->f[j].len && !memcmp(a->f[i].bytes, b->f[j].bytes, a->f[i].len);
    if (!found) return false;
  }
  return true;
}
UNUSED static const snap_file* find_file(const snap* s, const char* name) {
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->f[i].name, name)) return &s->f[i];
  return NULL;
}

/* ------------------------------------------------------------------ the app's clock and entropy */

static uint64_t g_now = 1700000000000ull;
static int g_seed_n;
static char g_seeds[512][PTX_SEED_MAX + 1];
static int g_nested;      /* a hook that calls gh_buy while a purchase is being made */
static gh_status g_nested_status = GH_NOTHING;

UNUSED static uint64_t t_now(void) {
  if (g_nested) {
    gh_outcome o;
    gh_buy(0, 0, &o);
    g_nested_status = o.status;
  }
  return g_now;
}
UNUSED static void t_seed(char out[PTX_SEED_MAX + 1]) {
  const int i = ++g_seed_n;
  snprintf(out, PTX_SEED_MAX + 1, "seed-%d", i);
  if (i < 512) memcpy(g_seeds[i], out, PTX_SEED_MAX + 1);
}

static gc_system g_scratch, g_before;
static uint8_t g_buf[1024 * 1024];
static uint8_t g_buf2[1024 * 1024];

UNUSED static uint64_t live_digest(void) {
  const size_t n = sysd_serialize(g_buf, sizeof g_buf);
  return n ? sysd_digest(g_buf, n) : 0;
}
UNUSED static uint64_t sys_digest(const gc_system* s) {
  const size_t n = gc_system_save(s, g_buf2, sizeof g_buf2);
  return n ? sysd_digest(g_buf2, n) : 0;
}
/* the digest of system.sav as the card holds it (0: no file) */
UNUSED static uint64_t card_digest(void) {
  char* b = NULL;
  size_t n = 0;
  if (save_read("system", &b, &n, NULL) != SAVE_OK) return 0;
  static gc_system card;
  const char* why;
  const bool ok = gc_system_load(&card, b, n, &why);
  free(b);
  return ok ? sys_digest(&card) : 1;
}

UNUSED static bool egg_eq(const gc_egg* a, const gc_egg* b) {
  return a->id == b->id && a->hatch_waves == b->hatch_waves && a->tier == b->tier && a->species == b->species && a->source_type == b->source_type &&
         a->variant_tier == b->variant_tier && a->egg_move_index == b->egg_move_index && a->is_shiny == b->is_shiny &&
         a->override_hidden_ability == b->override_hidden_ability && a->timestamp_ms == b->timestamp_ms;
}
UNUSED static bool result_eq(const ptx_result* a, const ptx_result* b) {
  if (a->txn != b->txn || a->kind != b->kind || a->outcome != b->outcome || a->n != b->n || strcmp(a->key, b->key)) return false;
  for (int i = 0; i < a->n; i++)
    if (a->order[i] != b->order[i] || !egg_eq(&a->eggs[i], &b->eggs[i])) return false;
  return true;
}

UNUSED static void reboot(void) {
  no_faults();
  sysd_reset();
  cs_reset();
  ptx_reset();
  gh_reset();
  ptx_boot();
}

/* A new card: a profile with `v` of every voucher and `eggs` eggs, no purchase record. */
UNUSED static void fresh(const uint32_t v[4], int eggs) {
  no_faults();
  wipe();
  sysd_reset();
  cs_reset();
  ptx_reset();
  gh_reset();
  gc_system* s = sysd_get();
  gc_system_init_defaults(s);
  for (int i = 0; i < 4; i++) s->voucher_counts[i] = v[i];
  s->dex[25].seen_attr = s->dex[25].caught_attr = 3;
  s->starters[25].candy_count = 7;
  s->stats.highest_wave = 77;
  for (int i = 0; i < eggs; i++) gc_system_add_egg(s, 1000 + (uint32_t)i, 10, (uint8_t)(i % 3), (uint16_t)(1 + i % 150));
  CHECK(sysd_save(), "the profile is saved");
  ptx_boot();
  g_seed_n = 0;
  g_nested = 0;
}
UNUSED static void fresh_all(uint32_t v, int eggs) {
  const uint32_t a[4] = {v, v, v, v};
  fresh(a, eggs);
}

/* What game-core gives for the same inputs a purchase was made with. */
UNUSED static void sow(pr_rng* r, const char* t) {
  pr_seedstr s;
  pr_seed_from_ascii(&s, t);
  pr_rng_sow(r, &s);
}
UNUSED static bool oracle(const gc_system* before, int machine, int option, const char* entropy_text, const char* scene_text, gc_system* after,
                   gc_gacha_result* gr, const char** why) {
  *after = *before;
  gc_gacha_ctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.machine = machine;
  ctx.cursor = option;
  ctx.timestamp_ms = g_now;
  sow(&ctx.entropy, entropy_text);
  sow(&ctx.scene_rng, scene_text);
  memset(gr, 0, sizeof *gr);
  return gc_system_gacha_pull(after, &ctx, gr, why);
}

static const struct {
  uint8_t voucher, cost, pulls;
} EXPECT[5] = {{0, 1, 1}, {0, 10, 10}, {1, 1, 5}, {2, 1, 10}, {3, 1, 25}};

#endif
