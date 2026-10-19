/* win_gains_test: what a Classic win gave (source/win_gains.c), against the real game-core: the system data is changed by
 * gc_classic_finish, and wg_collect has to name exactly what changed, in upstream's order.
 *
 *   make win_gains_test && build/win_gains_test
 *
 * Every case is checked two ways: against the list upstream's GameOverPhase would queue for that party, and against the system data itself
 * (the number of ribbon messages is the number of Egg Voucher Plus gained and of ribbons counted, the premium and golden vouchers are the
 * counts that went up, every unlock and achievement that changed is in the list, nothing else is). The words are checked against a fake
 * i18n_t that writes the key and its arguments. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../game-core/src/internal.h"
#include "pr_enums.h"
#include "win_gains.h"

static int checks, failed;
#define CHECK(cond, ...)                               \
  do {                                                 \
    checks++;                                          \
    if (!(cond)) {                                     \
      failed++;                                        \
      if (failed < 40) {                               \
        printf("FAIL line %d: %s: ", __LINE__, #cond); \
        printf(__VA_ARGS__);                           \
        printf("\n");                                  \
      }                                                \
    }                                                  \
  } while (0)

/* ------------------------------------------------------------------ fakes: i18n writes "key{name=value,...}" */

const char* i18n_t(char* out, int n, const char* key, ...) {
  va_list ap;
  va_start(ap, key);
  int len = snprintf(out, (size_t)n, "%s", key);
  const char* name;
  bool first = true;
  while ((name = va_arg(ap, const char*)) != NULL) {
    const char* value = va_arg(ap, const char*);
    if (len < n) len += snprintf(out + len, (size_t)(n - len), "%c%s=%s", first ? '{' : ',', name, value);
    first = false;
  }
  if (!first && len < n) snprintf(out + len, (size_t)(n - len), "}");
  va_end(ap);
  return out;
}
const char* i18n_species_name(int id) {
  static char buf[8][24];
  static int k;
  char* b = buf[k++ & 7];
  snprintf(b, 24, "Species%d", id);
  return b;
}

/* ------------------------------------------------------------------ helpers */

static gc_run R;
static gc_system SYS, BEFORE;
static wg_gains G;

/* A Classic run at wave 200 whose party is `species` (one-point starters first, for the starter budget, turned into them in place) */
static void begin(const int* species, int n) {
  static const int cheap[6] = {SPECIES_ID_CATERPIE, SPECIES_ID_WEEDLE, SPECIES_ID_PIDGEY, SPECIES_ID_RATTATA, SPECIES_ID_SPEAROW, SPECIES_ID_ZUBAT};
  gc_run_new(&R, GAME_MODES_CLASSIC, "wingains00000000000001", 7);
  gc_starter team[6];
  memset(team, 0, sizeof team);
  for (int i = 0; i < n; i++) {
    team[i].species = (uint16_t)cheap[i];
    team[i].tera_type = -1;
    for (int k = 0; k < 6; k++) team[i].ivs[k] = 15;
  }
  const char* why = NULL;
  CHECK(gc_run_set_starters(&R, team, n, &why), "starters: %s", why ? why : "?");
  for (int i = 0; i < n; i++) {
    R.party[i].b.species = (uint16_t)species[i];
    R.party[i].b.form = 0;
  }
  R.sys = &SYS;
  R.wave = 200;
}

static int count_kind(const wg_gains* g, int kind) {
  int c = 0;
  for (int i = 0; i < g->n; i++) c += g->item[i].kind == kind;
  return c;
}
static bool has(const wg_gains* g, int kind, int id) {
  for (int i = 0; i < g->n; i++)
    if (g->item[i].kind == kind && g->item[i].id == id) return true;
  return false;
}

/* what the system data says changed, against what wg_collect named */
static void invariants(const char* what) {
  const int plus = (int)(SYS.voucher_counts[GC_VOUCHER_PLUS] - BEFORE.voucher_counts[GC_VOUCHER_PLUS]);
  CHECK(count_kind(&G, WG_FIRST_RIBBON) == plus, "%s: %d ribbon messages for %d Egg Vouchers Plus", what, count_kind(&G, WG_FIRST_RIBBON), plus);
  CHECK(count_kind(&G, WG_FIRST_RIBBON) == (int)(SYS.stats.ribbons_owned - BEFORE.stats.ribbons_owned), "%s: ribbons counted", what);
  CHECK(count_kind(&G, WG_PREMIUM) == (int)(SYS.voucher_counts[GC_VOUCHER_PREMIUM] - BEFORE.voucher_counts[GC_VOUCHER_PREMIUM]), "%s: premium", what);
  CHECK(count_kind(&G, WG_VOUCHER_TOAST) == (int)(SYS.voucher_counts[GC_VOUCHER_GOLDEN] - BEFORE.voucher_counts[GC_VOUCHER_GOLDEN]), "%s: golden", what);
  CHECK(SYS.voucher_counts[GC_VOUCHER_REGULAR] == BEFORE.voucher_counts[GC_VOUCHER_REGULAR], "%s: regular vouchers untouched", what);
  int unlocks = 0;
  unlocks += SYS.unlocks.endless_mode != BEFORE.unlocks.endless_mode;
  unlocks += SYS.unlocks.spliced_endless_mode != BEFORE.unlocks.spliced_endless_mode;
  unlocks += SYS.unlocks.mini_black_hole != BEFORE.unlocks.mini_black_hole;
  unlocks += SYS.unlocks.eviolite != BEFORE.unlocks.eviolite;
  CHECK(count_kind(&G, WG_UNLOCK) == unlocks, "%s: %d unlock messages for %d unlocks", what, count_kind(&G, WG_UNLOCK), unlocks);
  int achv = 0;
  for (int id = GC_ACHV_CLASSIC_VICTORY; id <= GC_ACHV_100_RIBBONS; id++) {
    const bool now = SYS.achievement_flags[id] != 0, was = BEFORE.achievement_flags[id] != 0;
    achv += now && !was;
    CHECK(!(was && !now), "%s: an achievement was lost", what);
    CHECK(has(&G, WG_ACHV, id) == (now && !was), "%s: achievement %d", what, id);
  }
  CHECK(count_kind(&G, WG_ACHV) == achv, "%s: achievements", what);
  /* every item is bounded and the kinds are known */
  CHECK(G.n >= 0 && G.n <= WG_MAX, "%s: %d items", what, G.n);
  for (int i = 0; i < G.n; i++) CHECK(G.item[i].kind <= WG_PREMIUM, "%s: kind %d", what, G.item[i].kind);
}

/* snapshot, the win as game-core does it, collect */
static int win(const char* what) {
  wg_snapshot(&SYS);
  BEFORE = SYS;
  const char* why = NULL;
  CHECK(gc_classic_finish(&R, true, &why), "%s: the win is refused: %s", what, why ? why : "?");
  const int n = wg_collect(&SYS, &R, &G);
  CHECK(n == G.n, "%s: the return value is the count", what);
  invariants(what);
  return n;
}

/* the exact list, kinds and ids in order */
static void expect(const char* what, const wg_item* want, int n) {
  CHECK(G.n == n, "%s: %d items, wanted %d", what, G.n, n);
  for (int i = 0; i < n && i < G.n; i++)
    CHECK(G.item[i].kind == want[i].kind && G.item[i].id == want[i].id, "%s: item %d is (%d, %d), wanted (%d, %d)", what, i, G.item[i].kind,
          G.item[i].id, want[i].kind, want[i].id);
}

#define ITEM(k, i) {(k), (i)}

/* ------------------------------------------------------------------ the cases */

static void test_first_win_unevolved(void) {
  gc_system_init_defaults(&SYS);
  const int party[1] = {SPECIES_ID_BULBASAUR};
  begin(party, 1);
  win("first win, Bulbasaur");
  /* Undefeated and its Golden Voucher, Bring Your Child To Work Day (Bulbasaur evolves), Endless Mode, Mini Black Hole, Eviolite, Bulbasaur's first ribbon */
  const wg_item want[] = {ITEM(WG_ACHV, GC_ACHV_CLASSIC_VICTORY),
                          ITEM(WG_VOUCHER_TOAST, GC_VOUCHER_GOLDEN),
                          ITEM(WG_ACHV, GC_ACHV_UNEVOLVED_CLASSIC),
                          ITEM(WG_UNLOCK, WG_U_ENDLESS),
                          ITEM(WG_UNLOCK, WG_U_MINI_BLACK_HOLE),
                          ITEM(WG_UNLOCK, WG_U_EVIOLITE),
                          ITEM(WG_FIRST_RIBBON, SPECIES_ID_BULBASAUR)};
  expect("first win, Bulbasaur", want, 7);
  CHECK(SYS.voucher_counts[GC_VOUCHER_GOLDEN] == 1 && SYS.voucher_counts[GC_VOUCHER_PLUS] == 1 && SYS.voucher_counts[GC_VOUCHER_PREMIUM] == 0,
        "the first win gave a Golden and a Plus voucher and no Premium one");
}

static void test_second_win_same_species(void) {
  /* the system data of the first win goes on */
  const int party[1] = {SPECIES_ID_BULBASAUR};
  begin(party, 1);
  win("second win, Bulbasaur again");
  const wg_item want[] = {ITEM(WG_PREMIUM, 0)};
  expect("second win, Bulbasaur again", want, 1);
}

static void test_second_win_new_species(void) {
  const int party[1] = {SPECIES_ID_CHARMANDER};
  begin(party, 1);
  win("second win, Charmander");
  const wg_item want[] = {ITEM(WG_FIRST_RIBBON, SPECIES_ID_CHARMANDER), ITEM(WG_PREMIUM, 0)};
  expect("second win, Charmander", want, 2);
}

static void test_evolved_and_fused(void) {
  gc_system_init_defaults(&SYS);
  /* Venusaur does not evolve: no Bring Your Child To Work Day, no Eviolite; its ribbon is its root's (Bulbasaur). A fused Pokemon unlocks the spliced mode. */
  const int party[2] = {SPECIES_ID_VENUSAUR, SPECIES_ID_PIKACHU};
  begin(party, 2);
  R.party[1].b.fusion_species = SPECIES_ID_RAICHU;
  R.party[1].b.fusion_form = 0;
  win("first win, Venusaur and a fused Pikachu");
  CHECK(has(&G, WG_ACHV, GC_ACHV_CLASSIC_VICTORY), "Undefeated");
  /* Pikachu evolves (into Raichu): that is the unevolved one */
  CHECK(has(&G, WG_ACHV, GC_ACHV_UNEVOLVED_CLASSIC), "Pikachu can still evolve");
  int pos_endless = -1, pos_spliced = -1, pos_hole = -1, pos_evi = -1;
  for (int i = 0; i < G.n; i++) {
    if (G.item[i].kind != WG_UNLOCK) continue;
    if (G.item[i].id == WG_U_ENDLESS) pos_endless = i;
    if (G.item[i].id == WG_U_SPLICED_ENDLESS) pos_spliced = i;
    if (G.item[i].id == WG_U_MINI_BLACK_HOLE) pos_hole = i;
    if (G.item[i].id == WG_U_EVIOLITE) pos_evi = i;
  }
  CHECK(pos_endless >= 0 && pos_spliced > pos_endless && pos_hole > pos_spliced && pos_evi > pos_hole, "unlocks in upstream's order: %d %d %d %d", pos_endless,
        pos_spliced, pos_hole, pos_evi);
  /* the ribbons, in party order: Venusaur's root, then Pikachu's (a starter of its own, or the root) */
  CHECK(has(&G, WG_FIRST_RIBBON, SPECIES_ID_BULBASAUR), "Venusaur's ribbon is Bulbasaur's");
  const int before_unlocks = G.n;
  (void)before_unlocks;
}

static void test_duplicates_count_once(void) {
  gc_system_init_defaults(&SYS);
  const int party[2] = {SPECIES_ID_BULBASAUR, SPECIES_ID_BULBASAUR};
  begin(party, 2);
  win("two Bulbasaur");
  CHECK(count_kind(&G, WG_FIRST_RIBBON) == 1, "one species line, one ribbon (%d)", count_kind(&G, WG_FIRST_RIBBON));
  CHECK(SYS.starters[SPECIES_ID_BULBASAUR].classic_win_count == 2, "its count went to 2");
}

static void test_ribbon_milestone(void) {
  gc_system_init_defaults(&SYS);
  /* nine ribbons already: two new species lines make eleven, and the 10 ribbons achievement comes after Undefeated and the unevolved one */
  SYS.stats.ribbons_owned = 9;
  const int party[2] = {SPECIES_ID_PIDGEY, SPECIES_ID_RATTATA};
  begin(party, 2);
  win("ribbons 9 -> 11");
  CHECK(has(&G, WG_ACHV, GC_ACHV_10_RIBBONS) && !has(&G, WG_ACHV, GC_ACHV_25_RIBBONS), "the 10 ribbons achievement and not the 25");
  int p_unevolved = -1, p_ribbons = -1, p_unlock = -1;
  for (int i = 0; i < G.n; i++) {
    if (G.item[i].kind == WG_ACHV && G.item[i].id == GC_ACHV_UNEVOLVED_CLASSIC) p_unevolved = i;
    if (G.item[i].kind == WG_ACHV && G.item[i].id == GC_ACHV_10_RIBBONS) p_ribbons = i;
    if (G.item[i].kind == WG_UNLOCK && p_unlock < 0) p_unlock = i;
  }
  CHECK(p_unevolved >= 0 && p_ribbons > p_unevolved && p_unlock > p_ribbons, "bars before messages: %d %d %d", p_unevolved, p_ribbons, p_unlock);
}

/* a species whose starter root is not its root: game-core and this module have to agree on the order root, then starter */
static void test_two_roots(void) {
  int sp = 0;
  for (int s = 1; s < 1200 && !sp; s++) {
    if (!gc_species(s)) continue;
    if (gc_species_root_id((uint16_t)s) != gc_species_root_for_starter((uint16_t)s)) sp = s;
  }
  if (!sp) {
    printf("win_gains_test: no species with two roots in the data (case skipped)\n");
    return;
  }
  gc_system_init_defaults(&SYS);
  const int party[1] = {sp};
  begin(party, 1);
  win("a species with two roots");
  CHECK(count_kind(&G, WG_FIRST_RIBBON) == 2, "%d ribbon messages for species %d", count_kind(&G, WG_FIRST_RIBBON), sp);
  int first = -1, second = -1;
  for (int i = 0; i < G.n; i++)
    if (G.item[i].kind == WG_FIRST_RIBBON) {
      if (first < 0)
        first = G.item[i].id;
      else
        second = G.item[i].id;
    }
  CHECK(first == gc_species_root_id((uint16_t)sp) && second == gc_species_root_for_starter((uint16_t)sp), "root %d then starter %d (got %d, %d)",
        gc_species_root_id((uint16_t)sp), gc_species_root_for_starter((uint16_t)sp), first, second);
}

static void test_nothing_to_tell(void) {
  gc_system_init_defaults(&SYS);
  const int party[1] = {SPECIES_ID_BULBASAUR};
  begin(party, 1);
  /* no snapshot (a run from rundriver.txt has no system data) */
  wg_snapshot(NULL);
  CHECK(gc_classic_finish(&R, true, NULL), "the win");
  CHECK(wg_collect(&SYS, &R, &G) == 0 && G.n == 0, "no snapshot, no gains");
  /* not a Classic run */
  gc_system_init_defaults(&SYS);
  begin(party, 1);
  wg_snapshot(&SYS);
  R.run.mode = GAME_MODES_ENDLESS;
  SYS.unlocks.endless_mode = true; /* even if the data differs */
  CHECK(wg_collect(&SYS, &R, &G) == 0, "an Endless run tells nothing");
  /* a run that was not won: the data is as it was */
  gc_system_init_defaults(&SYS);
  begin(party, 1);
  wg_snapshot(&SYS);
  CHECK(gc_classic_finish(&R, false, NULL), "a loss is a finished run");
  CHECK(wg_collect(&SYS, &R, &G) == 0, "a loss changes nothing");
  /* NULL run and NULL system */
  CHECK(wg_collect(NULL, &R, &G) == 0 && wg_collect(&SYS, NULL, &G) == 0, "NULL arguments");
}

static void test_words(void) {
  wg_toast t;
  wg_item it = ITEM(WG_ACHV, GC_ACHV_CLASSIC_VICTORY);
  CHECK(wg_is_toast(&it), "an achievement is a bar");
  wg_toast_of(&it, &t);
  CHECK(!strcmp(t.title, "achv:classicVictory.name") && !strcmp(t.desc, "achv:classicVictory.description") && !strcmp(t.score, "+250pt") &&
            !strcmp(t.icon, "classic_ribbon_default") && t.tier == 4,
        "Undefeated: %s / %s / %s / %s / %d", t.title, t.desc, t.score, t.icon, t.tier);
  it = (wg_item)ITEM(WG_ACHV, GC_ACHV_UNEVOLVED_CLASSIC);
  wg_toast_of(&it, &t);
  CHECK(!strcmp(t.title, "achv:unevolvedClassicVictory.name") && !strcmp(t.score, "+50pt") && !strcmp(t.icon, "eviolite") && t.tier == 2, "Bring Your Child To Work Day");
  static const struct {
    int id, ribbons, score, tier;
    const char* icon;
  } rib[5] = {{GC_ACHV_10_RIBBONS, 10, 50, 2, "common_ribbon"},
              {GC_ACHV_25_RIBBONS, 25, 75, 3, "great_ribbon"},
              {GC_ACHV_50_RIBBONS, 50, 100, 4, "ultra_ribbon"},
              {GC_ACHV_75_RIBBONS, 75, 125, 4, "rogue_ribbon"},
              {GC_ACHV_100_RIBBONS, 100, 150, 4, "master_ribbon"}};
  for (int i = 0; i < 5; i++) {
    it = (wg_item)ITEM(WG_ACHV, rib[i].id);
    wg_toast_of(&it, &t);
    char want[96], sc[16];
    snprintf(want, sizeof want, "achv:ribbonAchv.description{ribbonAmount=%d}", rib[i].ribbons);
    snprintf(sc, sizeof sc, "+%dpt", rib[i].score);
    CHECK(!strcmp(t.desc, want) && !strcmp(t.score, sc) && !strcmp(t.icon, rib[i].icon) && t.tier == rib[i].tier, "%d ribbons: %s / %s / %s / %d",
          rib[i].ribbons, t.desc, t.score, t.icon, t.tier);
  }
  it = (wg_item)ITEM(WG_VOUCHER_TOAST, GC_VOUCHER_GOLDEN);
  CHECK(wg_is_toast(&it), "the voucher is a bar");
  wg_toast_of(&it, &t);
  CHECK(!strcmp(t.title, "voucher:eggVoucherGold") && !strcmp(t.desc, "achv:classicVictory.description") && t.score[0] == 0 &&
            !strcmp(t.icon, "golden_mystic_ticket") && t.tier == 3,
        "the Golden voucher bar: %s / %s / %s / %d", t.title, t.desc, t.icon, t.tier);

  char m[512];
  it = (wg_item)ITEM(WG_UNLOCK, WG_U_ENDLESS);
  CHECK(!wg_is_toast(&it), "an unlock is a message");
  wg_message_of(&it, m, sizeof m);
  CHECK(!strcmp(m, "battle:unlockedSomething{unlockedThing=gameMode:mode{mode=gameMode:endless}}"), "Endless: %s", m);
  it = (wg_item)ITEM(WG_UNLOCK, WG_U_SPLICED_ENDLESS);
  wg_message_of(&it, m, sizeof m);
  CHECK(!strcmp(m, "battle:unlockedSomething{unlockedThing=gameMode:mode{mode=gameMode:endlessSpliced}}"), "Spliced: %s", m);
  it = (wg_item)ITEM(WG_UNLOCK, WG_U_MINI_BLACK_HOLE);
  wg_message_of(&it, m, sizeof m);
  CHECK(!strcmp(m, "battle:unlockedSomething{unlockedThing=modifierType:ModifierType.MINI_BLACK_HOLE.name}"), "Mini Black Hole: %s", m);
  it = (wg_item)ITEM(WG_UNLOCK, WG_U_EVIOLITE);
  wg_message_of(&it, m, sizeof m);
  CHECK(!strcmp(m, "battle:unlockedSomething{unlockedThing=modifierType:ModifierType.EVIOLITE.name}"), "Eviolite: %s", m);
  it = (wg_item)ITEM(WG_FIRST_RIBBON, SPECIES_ID_CHARMANDER);
  wg_message_of(&it, m, sizeof m);
  char want[512];
  snprintf(want, sizeof want,
           "battle:beatModeFirstTime{speciesName=Species%d,gameMode=gameMode:classic,newModifier=modifierType:ModifierType.AddVoucherModifierType.name{modifierCount=1,"
           "voucherTypeName=voucher:eggVoucherPlus}}",
           SPECIES_ID_CHARMANDER);
  CHECK(!strcmp(m, want), "first ribbon: %s", m);
  it = (wg_item)ITEM(WG_PREMIUM, 0);
  wg_message_of(&it, m, sizeof m);
  CHECK(!strcmp(m,
                "battle:rewardGain{modifierName=modifierType:ModifierType.AddVoucherModifierType.name{modifierCount=1,voucherTypeName=voucher:eggVoucherPremium}}"),
        "Premium: %s", m);
}

int main(void) {
  test_first_win_unevolved();
  test_second_win_same_species();
  test_second_win_new_species();
  test_evolved_and_fused();
  test_duplicates_count_once();
  test_ribbon_milestone();
  test_two_roots();
  test_nothing_to_tell();
  test_words();
  printf("win_gains_test: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
