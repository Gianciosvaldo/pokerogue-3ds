/* What a Classic win gave the player (see win_gains.h). */
#include "win_gains.h"

#include <stdio.h>
#include <string.h>

#include "gc/run.h"
#include "i18n.h"
#include "pr_enums.h"

/* the system data's fields a win changes, as they were when the run started */
static struct {
  bool valid;
  gc_unlocks unlocks;
  uint32_t voucher_counts[4];
  uint8_t achv[GC_ACHV_100_RIBBONS - GC_ACHV_CLASSIC_VICTORY + 1];
  uint16_t win_count[GC_MAX_SPECIES_ID + 1];
} B;

void wg_snapshot(const gc_system* sys) {
  B.valid = sys != NULL;
  if (!sys) return;
  B.unlocks = sys->unlocks;
  memcpy(B.voucher_counts, sys->voucher_counts, sizeof B.voucher_counts);
  for (int i = 0; i < (int)sizeof B.achv; i++) B.achv[i] = sys->achievement_flags[GC_ACHV_CLASSIC_VICTORY + i];
  for (int i = 0; i <= GC_MAX_SPECIES_ID; i++) B.win_count[i] = sys->starters[i].classic_win_count;
}

static bool new_achv(const gc_system* a, int id) {
  return a->achievement_flags[id] && !B.achv[id - GC_ACHV_CLASSIC_VICTORY];
}

static void add(wg_gains* g, wg_kind k, int id) {
  if (g->n >= WG_MAX) return;
  g->item[g->n].kind = (uint8_t)k;
  g->item[g->n].id = (uint16_t)id;
  g->n++;
}

int wg_collect(const gc_system* a, const struct gc_run* run, wg_gains* out) {
  out->n = 0;
  if (!B.valid || !a || !run || run->run.mode != GAME_MODES_CLASSIC) return 0;
  /* validateAchv(CLASSIC_VICTORY): the bar, then the voucher that achievement unlocks */
  if (new_achv(a, GC_ACHV_CLASSIC_VICTORY)) {
    add(out, WG_ACHV, GC_ACHV_CLASSIC_VICTORY);
    if (a->voucher_counts[GC_VOUCHER_GOLDEN] > B.voucher_counts[GC_VOUCHER_GOLDEN]) add(out, WG_VOUCHER_TOAST, GC_VOUCHER_GOLDEN);
  }
  if (new_achv(a, GC_ACHV_UNEVOLVED_CLASSIC)) add(out, WG_ACHV, GC_ACHV_UNEVOLVED_CLASSIC);
  /* the ribbon milestones, as the ribbons are counted */
  for (int id = GC_ACHV_10_RIBBONS; id <= GC_ACHV_100_RIBBONS; id++)
    if (new_achv(a, id)) add(out, WG_ACHV, id);
  /* handleUnlocks */
  if (a->unlocks.endless_mode && !B.unlocks.endless_mode) add(out, WG_UNLOCK, WG_U_ENDLESS);
  if (a->unlocks.spliced_endless_mode && !B.unlocks.spliced_endless_mode) add(out, WG_UNLOCK, WG_U_SPLICED_ENDLESS);
  if (a->unlocks.mini_black_hole && !B.unlocks.mini_black_hole) add(out, WG_UNLOCK, WG_U_MINI_BLACK_HOLE);
  if (a->unlocks.eviolite && !B.unlocks.eviolite) add(out, WG_UNLOCK, WG_U_EVIOLITE);
  /* awardFirstClassicCompletion: each party member's root species, then its starter's when that is another one, whose win count went from
   * 0 to something. A species line twice in the party counts once (its count was 0 only the first time). */
  uint16_t seen[12];
  int n_seen = 0;
  for (int k = 0; k < run->n_party && k < 6; k++) {
    const int j = run->party_order[k];
    if (j < 0 || j >= run->n_party) continue;
    const uint16_t sp = run->party[j].b.species;
    const uint16_t roots[2] = {gc_species_root_id(sp), gc_species_root_for_starter(sp)};
    for (int r = 0; r < 2; r++) {
      const uint16_t root = roots[r];
      if (r == 1 && root == roots[0]) continue;
      if (root < 1 || root > GC_MAX_SPECIES_ID || B.win_count[root] != 0 || a->starters[root].classic_win_count == 0) continue;
      bool dup = false;
      for (int i = 0; i < n_seen; i++)
        if (seen[i] == root) dup = true;
      if (dup || n_seen >= 12) continue;
      seen[n_seen++] = root;
      add(out, WG_FIRST_RIBBON, root);
    }
  }
  /* GameOverModifierRewardPhase: the Premium Voucher of a win that is not the first */
  if (a->voucher_counts[GC_VOUCHER_PREMIUM] > B.voucher_counts[GC_VOUCHER_PREMIUM]) add(out, WG_PREMIUM, 0);
  return out->n;
}

/* ------------------------------------------------------------------ the words */

typedef struct {
  const char* key;    /* achv:<key>.name / .description */
  const char* icon;
  int score;
  int ribbons;        /* RibbonAchv: the amount in its description; 0 = a plain description */
} achv_row;

static const achv_row ACHV[] = {
    {"classicVictory", "classic_ribbon_default", 250, 0},   /* GC_ACHV_CLASSIC_VICTORY */
    {"unevolvedClassicVictory", "eviolite", 50, 0},         /* GC_ACHV_UNEVOLVED_CLASSIC */
    {"10Ribbons", "common_ribbon", 50, 10},
    {"25Ribbons", "great_ribbon", 75, 25},
    {"50Ribbons", "ultra_ribbon", 100, 50},
    {"75Ribbons", "rogue_ribbon", 125, 75},
    {"100Ribbons", "master_ribbon", 150, 100},
};

/* upstream getVoucherTypeIcon / getVoucherTypeName keys */
static const char* const VOUCHER_ICON[4] = {"coupon", "pair_of_tickets", "mystic_ticket", "golden_mystic_ticket"};
static const char* const VOUCHER_NAME[4] = {"voucher:eggVoucher", "voucher:eggVoucherPlus", "voucher:eggVoucherPremium", "voucher:eggVoucherGold"};

/* Achv.getTier by score */
static int achv_tier(int score) { return score >= 100 ? 4 : score >= 75 ? 3 : score >= 50 ? 2 : score >= 25 ? 1 : 0; }

bool wg_is_toast(const wg_item* it) { return it->kind == WG_ACHV || it->kind == WG_VOUCHER_TOAST; }

void wg_toast_of(const wg_item* it, wg_toast* o) {
  memset(o, 0, sizeof *o);
  if (it->kind == WG_ACHV && it->id >= GC_ACHV_CLASSIC_VICTORY && it->id <= GC_ACHV_100_RIBBONS) {
    const achv_row* r = &ACHV[it->id - GC_ACHV_CLASSIC_VICTORY];
    char key[64], n[16];
    snprintf(key, sizeof key, "achv:%s.name", r->key);
    i18n_t(o->title, sizeof o->title, key, NULL);
    if (r->ribbons) {
      snprintf(n, sizeof n, "%d", r->ribbons);
      i18n_t(o->desc, sizeof o->desc, "achv:ribbonAchv.description", "ribbonAmount", n, NULL);
    } else {
      snprintf(key, sizeof key, "achv:%s.description", r->key);
      i18n_t(o->desc, sizeof o->desc, key, NULL);
    }
    snprintf(o->score, sizeof o->score, "+%dpt", r->score);
    o->icon = r->icon;
    o->tier = achv_tier(r->score);
  } else if (it->kind == WG_VOUCHER_TOAST && it->id < 4) {
    /* initVouchers: the voucher of the Classic victory achievement carries its description, and its tier is the voucher's (GOLDEN = ROGUE) */
    i18n_t(o->title, sizeof o->title, VOUCHER_NAME[it->id], NULL);
    i18n_t(o->desc, sizeof o->desc, "achv:classicVictory.description", NULL);
    o->icon = VOUCHER_ICON[it->id];
    o->tier = it->id == GC_VOUCHER_REGULAR ? 0 : it->id == GC_VOUCHER_PLUS ? 1 : it->id == GC_VOUCHER_PREMIUM ? 2 : 3;
  }
}

/* AddVoucherModifierType.name: "1x Egg Voucher Plus" */
static void voucher_name(char* out, int n, int type) {
  char vn[128];
  i18n_t(vn, sizeof vn, VOUCHER_NAME[type], NULL);
  i18n_t(out, n, "modifierType:ModifierType.AddVoucherModifierType.name", "modifierCount", "1", "voucherTypeName", vn, NULL);
}

void wg_message_of(const wg_item* it, char* out, int n) {
  char a[192], b[192], c[192];
  out[0] = 0;
  switch (it->kind) {
    case WG_UNLOCK: {
      /* getUnlockableName */
      switch (it->id) {
        case WG_U_ENDLESS:
          i18n_t(a, sizeof a, "gameMode:endless", NULL);
          i18n_t(b, sizeof b, "gameMode:mode", "mode", a, NULL);
          break;
        case WG_U_SPLICED_ENDLESS:
          i18n_t(a, sizeof a, "gameMode:endlessSpliced", NULL);
          i18n_t(b, sizeof b, "gameMode:mode", "mode", a, NULL);
          break;
        case WG_U_MINI_BLACK_HOLE:
          i18n_t(b, sizeof b, "modifierType:ModifierType.MINI_BLACK_HOLE.name", NULL);
          break;
        default:
          i18n_t(b, sizeof b, "modifierType:ModifierType.EVIOLITE.name", NULL);
          break;
      }
      i18n_t(out, n, "battle:unlockedSomething", "unlockedThing", b, NULL);
      break;
    }
    case WG_FIRST_RIBBON:
      /* RibbonModifierRewardPhase: gameMode is GameMode.getName() ("Classic"), the string adds " Mode" */
      i18n_t(a, sizeof a, "gameMode:classic", NULL);
      voucher_name(c, sizeof c, GC_VOUCHER_PLUS);
      i18n_t(out, n, "battle:beatModeFirstTime", "speciesName", i18n_species_name(it->id), "gameMode", a, "newModifier", c, NULL);
      break;
    case WG_PREMIUM:
      voucher_name(c, sizeof c, GC_VOUCHER_PREMIUM);
      i18n_t(out, n, "battle:rewardGain", "modifierName", c, NULL);
      break;
    default:
      break;
  }
}
