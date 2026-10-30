/* A Mystery Encounter's texts, question and picks (see me_flow.h). */
#include "me_flow.h"

#include <stdio.h>
#include <string.h>

#include "battle_events.h"
#include "i18n.h"
#include "me_screen.h"
#include "gm_item_ui.h"
#include "pr_data.h"

/* locale namespace per gc_mystery_type */
static const char* const NS[GC_ME_COUNT] = {
    [GC_ME_MYSTERIOUS_CHALLENGERS] = "mysteriousChallengers",
    [GC_ME_MYSTERIOUS_CHEST] = "mysteriousChest",
    [GC_ME_DARK_DEAL] = "darkDeal",
    [GC_ME_FIGHT_OR_FLIGHT] = "fightOrFlight",
    [GC_ME_SLUMBERING_SNORLAX] = "slumberingSnorlax",
    [GC_ME_TRAINING_SESSION] = "trainingSession",
    [GC_ME_DEPARTMENT_STORE_SALE] = "departmentStoreSale",
    [GC_ME_SHADY_VITAMIN_DEALER] = "shadyVitaminDealer",
    [GC_ME_FIELD_TRIP] = "fieldTrip",
    [GC_ME_SAFARI_ZONE] = "safariZone",
    [GC_ME_LOST_AT_SEA] = "lostAtSea",
    [GC_ME_FIERY_FALLOUT] = "fieryFallout",
    [GC_ME_THE_STRONG_STUFF] = "theStrongStuff",
    [GC_ME_THE_POKEMON_SALESMAN] = "thePokemonSalesman",
    [GC_ME_AN_OFFER_YOU_CANT_REFUSE] = "anOfferYouCantRefuse",
    [GC_ME_DELIBIRDY] = "delibirdy",
    [GC_ME_ABSOLUTE_AVARICE] = "absoluteAvarice",
    [GC_ME_A_TRAINERS_TEST] = "aTrainersTest",
    [GC_ME_TRASH_TO_TREASURE] = "trashToTreasure",
    [GC_ME_BERRIES_ABOUND] = "berriesAbound",
    [GC_ME_CLOWNING_AROUND] = "clowningAround",
    [GC_ME_PART_TIMER] = "partTimer",
    [GC_ME_DANCING_LESSONS] = "dancingLessons",
    [GC_ME_WEIRD_DREAM] = "weirdDream",
    [GC_ME_THE_WINSTRATE_CHALLENGE] = "theWinstrateChallenge",
    [GC_ME_TELEPORTING_HIJINKS] = "teleportingHijinks",
    [GC_ME_BUG_TYPE_SUPERFAN] = "bugTypeSuperfan",
    [GC_ME_FUN_AND_GAMES] = "funAndGames",
    [GC_ME_UNCOMMON_BREED] = "uncommonBreed",
    [GC_ME_GLOBAL_TRADE_SYSTEM] = "globalTradeSystem",
    [GC_ME_THE_EXPERT_POKEMON_BREEDER] = "theExpertPokemonBreeder",
    [GC_ME_PILLAR_MEN] = "pillarMen",
};

/* The Pillar Men are lorenzo's own encounter (custom runs): no upstream locale file, so their texts are here. The three silhouettes are
 * the trainer art of the three fighters, in the order of the options (easy, hard, brutal). */
static const char* const PILLAR_ID[3] = {"esidisi", "wamuu", "kars"};
static bool pillar_text(const char* sub, char* out, size_t n)
{
  static const struct { const char* sub; const char* text; } T[] = {
      {"intro", "Three shadows rise in the dark.$A Bizarre Foe appears!"},
      {"title", "The Pillar Men"},
      {"description", "Three ancient warriors bar the way, and each wants to test you."},
      {"query", "Whom will you face?"},
      {"option.1.label", "Esidisi"},
      {"option.1.tooltip", "(+) An easy fight (+) TMs and a Memory Mushroom"},
      {"option.1.selected", "You step up to Esidisi!"},
      {"option.2.label", "Wamuu"},
      {"option.2.tooltip", "(-) A hard fight (+) Ultra and Great rewards"},
      {"option.2.selected", "You step up to Wamuu!"},
      {"option.3.label", "Kars"},
      {"option.3.tooltip", "(-) A brutal fight (+) Rogue and Ultra rewards"},
      {"option.3.selected", "You step up to Kars!"},
  };
  for (size_t i = 0; i < sizeof T / sizeof *T; i++)
    if (!strcmp(T[i].sub, sub)) {
      snprintf(out, n, "%s", T[i].text);
      return true;
    }
  return false;
}

const char* me_flow_ns(int type)
{
  return type >= 0 && type < GC_ME_COUNT ? NS[type] : NULL;
}

#define MAX_EXTRA 4

static struct {
  const gc_run* run;
  int type;
  int option, member;                  /* the option taken, the party slot picked */
  int move;
  int trait[25], n_trait;              /* the natures or ability slots the list offers, by row */
  int pick_row[ME_MAX_OPTIONS], item_page; /* row -> move/item ordinal, or -1/-2 for previous/next */
  void (*on_choice)(const me_choice* c, void* arg);
  void* arg;
} F;

/* A {{token}} the screen does not carry (an encounter not ported with its texts yet) is worded as a plain phrase instead of showing its braces. */
static void strip_tokens(char* t, size_t n)
{
  char result[ME_TEXT_LEN * 2];
  size_t used = 0;
  if (n > sizeof result) n = sizeof result;
  if (!n) return;
  for (const char* r = t; *r;) {
    const char* e = r[0] == '{' && r[1] == '{' ? strstr(r, "}}") : NULL;
    if (!e) {
      if (used + 1 < n) result[used++] = *r;
      r++;
      continue;
    }
    char name[48];
    size_t l = (size_t)(e - r - 2);
    if (l >= sizeof name) l = sizeof name - 1;
    memcpy(name, r + 2, l);
    name[l] = 0;
    const char* rep = strstr(name, "Name") ? "a Pokemon" : (strstr(name, "oney") || strstr(name, "rice") || strstr(name, "ost")) ? "some money" : "?";
    for (; *rep && used + 1 < n; rep++) result[used++] = *rep;
    r = e + 2;
  }
  result[used] = 0;
  memcpy(t, result, used + 1);
}

/* The text at "mysteryEncounters/<ns>:<sub>" worded with the screen's tokens and `extra`. Returns false when the locale lacks the key,
 * or (with `complete`) when the text names a token nobody carries. */
static bool fmt(const char* sub, const pr_msg_arg* extra, int n_extra, bool complete, char* out, size_t n)
{
  out[0] = 0;
  if (F.type == GC_ME_PILLAR_MEN) return pillar_text(sub, out, n);
  const char* ns = me_flow_ns(F.type);
  if (!ns) return false;
  char key[160], raw[ME_TEXT_LEN * 2];
  snprintf(key, sizeof key, "mysteryEncounters/%s:%s", ns, sub);
  if (!pr_text_lookup(be_text_source(), key, raw, sizeof raw)) return false;
  pr_msg m = {.key = key};
  const gc_screen* s = gc_run_screen(F.run);
  for (const char* p = raw; (p = strstr(p, "{{")) != NULL;) {
    const char* name = p + 2;
    while (*name == ' ') name++;
    const char* e = name;
    while (*e && *e != ',' && *e != ' ' && *e != '}') e++;
    const size_t len = (size_t)(e - name);
    p = e;
    const pr_msg_arg* found = NULL;
    for (int i = 0; i < n_extra && !found; i++)
      if (strlen(extra[i].name) == len && !strncmp(extra[i].name, name, len)) found = &extra[i];
    for (int i = 0; i < s->n_me_args && !found; i++)
      if (strlen(s->me_args[i].name) == len && !strncmp(s->me_args[i].name, name, len)) found = &s->me_args[i];
    if (!found) {
      if (complete) return false;
      continue;
    }
    bool dup = false;
    for (int i = 0; i < m.n_args; i++) dup |= m.args[i].name == found->name;
    if (!dup && m.n_args < PR_MSG_MAX_ARGS) m.args[m.n_args++] = *found;
  }
  be_format_message(&m, out, n);
  strip_tokens(out, n);
  return out[0] != 0;
}

static void append_page(char* out, size_t n, const char* page)
{
  if (!page[0]) return;
  const size_t l = strlen(out);
  if (l + 2 >= n) return;
  snprintf(out + l, n - l, "%s%s", l ? "$" : "", page);
}

void me_flow_intro(const gc_run* run, char* out, size_t n)
{
  out[0] = 0;
  F.run = run;
  F.type = gc_run_screen(run)->mystery_type;
  char t[ME_TEXT_LEN * 2];
  if (fmt("intro", NULL, 0, false, t, sizeof t)) append_page(out, n, t);
  if (fmt("introDialogue", NULL, 0, false, t, sizeof t)) append_page(out, n, t);
}

/* ------------------------------------------------------------------ the intro sprites */

/* withIntroSpriteConfigs of each encounter (upstream 8555c08), in order. root: M = images/mystery-encounters, T = images/trainer, I = items
 * icon, P = images/pokemon; "" = a sprite picked at run time (a Pokemon, a trainer): it keeps its place in the spacing but is not drawn.
 * x and y as upstream has them (0 = not set); a sprite with neither is spaced evenly. */
typedef struct {
  char root;
  const char* name;
  int8_t x, y;
  float scale, dark;
} intro_cfg;
#define INTRO_MAX 13
static const struct {
  int type;
  int n;
  intro_cfg c[INTRO_MAX];
} INTRO[] = {
    {GC_ME_DARK_DEAL, 2, {{'M', "dark_deal_scientist", 0, 0, 0, 0}, {'M', "dark_deal_porygon", 0, 0, 0, 0}}},
    {GC_ME_MYSTERIOUS_CHEST, 1, {{'M', "mysterious_chest_blue", 0, 8, 0, 0}}},
    {GC_ME_DEPARTMENT_STORE_SALE, 2, {{'M', "department_store_sale_lady", -20, 0, 0, 0}, {0, "", 30, 0, 0, 0}}},
    {GC_ME_SHADY_VITAMIN_DEALER, 2, {{0, "", 24, 0, 1.1f, 0}, {'M', "shady_vitamin_dealer", -12, 3, 0, 0}}},
    {GC_ME_FIELD_TRIP, 3, {{'T', "preschooler_m", 0, 0, 0, 0}, {'M', "field_trip_teacher", 0, 0, 0, 0}, {'T', "preschooler_f", 0, 0, 0, 0}}},
    {GC_ME_SAFARI_ZONE, 1, {{'M', "safari_zone", 4, 6, 0, 0}}},
    {GC_ME_LOST_AT_SEA, 1, {{'M', "lost_at_sea_buoy", 20, 3, 0, 0}}},
    {GC_ME_THE_STRONG_STUFF, 2, {{'I', "berry_juice_good", -15, 3, 1.25f, 0}, {0, "", 20, 10, 1.25f, 0}}},
    {GC_ME_THE_POKEMON_SALESMAN, 2, {{'M', "pokemon_salesman", 0, 0, 0, 0}, {0, "", 0, 0, 0, 0}}},
    {GC_ME_AN_OFFER_YOU_CANT_REFUSE, 2, {{0, "", 0, -4, 0, 0}, {'T', "rich_kid_m", 2, 5, 0, 0}}},
    {GC_ME_ABSOLUTE_AVARICE, 13, {{0, "", -5, 0, 0, 0}, {0, "", -5, 0, 0, 0}, {'I', "lum_berry", 7, -14, 0, 0}, {'I', "salac_berry", 2, 4, 0, 0},
        {'I', "lansat_berry", 32, 5, 0, 0}, {'I', "liechi_berry", 6, -5, 0, 0}, {'I', "sitrus_berry", 7, 8, 0, 0},
        {'I', "enigma_berry", 26, -4, 0, 0}, {'I', "leppa_berry", 16, -27, 0, 0}, {'I', "petaya_berry", 30, -17, 0, 0},
        {'I', "ganlon_berry", 16, -11, 0, 0}, {'I', "apicot_berry", 14, -2, 0, 0}, {'I', "starf_berry", 18, 9, 0, 0}}},
    {GC_ME_TRASH_TO_TREASURE, 1, {{'P', "569-gigantamax", 0, 8, 1.5f, 0.4f}}},
    {GC_ME_BERRIES_ABOUND, 2, {{'M', "berries_abound_bush", 25, -6, 0, 0}, {0, "", -5, 0, 0, 0.25f}}},
    {GC_ME_CLOWNING_AROUND, 3, {{0, "", -25, -3, 0, 0.3f}, {0, "", 25, -3, 0, 0.3f}, {'T', "harlequin", 0, 2, 0, 0}}},
    {GC_ME_PART_TIMER, 2, {{'M', "part_timer_crate", 15, 6, 0, 0}, {'T', "worker_f", -18, 4, 0, 0}}},
    {GC_ME_WEIRD_DREAM, 1, {{'M', "weird_dream_woman", 4, 11, 0, 0}}},
    {GC_ME_THE_WINSTRATE_CHALLENGE, 5, {{'T', "vito", 16, -4, 0, 0}, {'T', "vivi", -14, -4, 0, 0}, {'T', "victor", -32, 0, 0, 0},
        {'T', "victoria", 40, 0, 0, 0}, {'T', "vicky", 3, 5, 0, 0}}},
    {GC_ME_TELEPORTING_HIJINKS, 1, {{'M', "teleporting_hijinks_teleporter", 4, 4, 0, 0}}},
    {GC_ME_BUG_TYPE_SUPERFAN, 2, {{0, "", 35, -2, 0, 0}, {'T', "bug_type_superfan", -20, 5, 0, 0}}},
    {GC_ME_FUN_AND_GAMES, 3, {{'M', "fun_and_games_game", 0, 6, 0, 0}, {'M', "fun_and_games_wobbuffet", -28, 6, 0, 0},
        {'M', "fun_and_games_man", 40, 6, 0, 0}}},
    {GC_ME_GLOBAL_TRADE_SYSTEM, 1, {{'M', "global_trade_system", 3, 5, 0, 0}}},
    {GC_ME_THE_EXPERT_POKEMON_BREEDER, 2, {{0, "", 14, -2, 0, 0}, {'T', "expert_pokemon_breeder", -14, 4, 0, 0}}},
    {GC_ME_TRAINING_SESSION, 1, {{'M', "training_session_gear", 5, 6, 0, 0}}},
};

int me_flow_intro_sprites(int type, float cx, float floor_y, me_intro_sprite* out, int max)
{
  for (size_t t = 0; t < sizeof INTRO / sizeof INTRO[0]; t++) {
    if (INTRO[t].type != type) continue;
    const int total = INTRO[t].n;
    int spaced = 0;
    for (int i = 0; i < total; i++) spaced += !INTRO[t].c[i].x && !INTRO[t].c[i].y;
    const int spacing = (80 + spaced / 2) / (spaced > 1 ? spaced : 1); /* Math.round((maxX - minX) / max(spaced, 1)) */
    int n = 0, k = 0;
    for (int i = 0; i < total; i++) {
      const intro_cfg* c = &INTRO[t].c[i];
      const bool item = c->root == 'I';
      float x = item ? -19.0f : 0.0f;
      if (c->x || c->y) {
        if (c->x) x = 4.0f + c->x;
      } else if (total == 1) {
        x = 4.0f;
      } else {
        x = -40.0f + ((float)k + 0.5f) * (float)spacing + 4.0f;
        k++;
      }
      if (!c->root || n >= max) continue;
      me_intro_sprite* o = &out[n++];
      memset(o, 0, sizeof *o);
      switch (c->root) {
      case 'M': snprintf(o->key, sizeof o->key, "images/mystery-encounters/%s", c->name); break;
      case 'T': snprintf(o->key, sizeof o->key, "images/trainer/%s", c->name); break;
      case 'P': snprintf(o->key, sizeof o->key, "images/pokemon/%s", c->name); break;
      default: snprintf(o->key, sizeof o->key, "items"); snprintf(o->frame, sizeof o->frame, "%s", c->name); break;
      }
      o->x = cx + x;
      o->y = floor_y + (item ? 2.0f : 0.0f) + (float)c->y;
      o->scale = c->scale > 0 ? c->scale : 1.0f;
      o->dark = c->dark;
    }
    return n;
  }
  return 0;
}

/* ------------------------------------------------------------------ the picks */

static void open_question(int cursor);
static void open_members(void);
static void open_moves(void);
static void open_trade_offers(void);
static void open_items(void);

static void finish(int option, int b, int c)
{
  me_choice ch = {.option = option, .b = b, .c = c};
  pr_msg_arg extra[4];
  int n_extra = 0;
  if (F.run && b >= 0 && b < F.run->n_party && F.run->screen.option_pick[option])
    extra[n_extra++] = pr_arg_pokemon_name(NULL, "selectedPokemon", &F.run->party[b].b);
  bool field_correct = false;
  if (F.type == GC_ME_FIELD_TRIP && b >= 0 && b < F.run->n_party && c >= 0 && c < F.run->party[b].b.n_moves) {
    const int move = F.run->party[b].b.moves[c];
    const int category[] = {MOVE_CATEGORY_PHYSICAL, MOVE_CATEGORY_SPECIAL, MOVE_CATEGORY_STATUS};
    const char* const names[] = {"mysteryEncounters/fieldTrip:physical", "mysteryEncounters/fieldTrip:special", "mysteryEncounters/fieldTrip:status"};
    extra[n_extra++] = pr_arg_pokemon_name(NULL, "pokeName", &F.run->party[b].b);
    extra[n_extra++] = pr_arg_move("move", move);
    extra[n_extra++] = pr_arg_key("moveCategory", names[option]);
    field_correct = pr_moves[move].category == category[option];
  }
  char sub[48], page[ME_TEXT_LEN * 2];
  /* Berries Abound: the race the boss wins has a text of its own */
  bool lost = false;
  if (F.run) {
    const gc_screen* sc = gc_run_screen(F.run);
    for (int i = 0; i < sc->n_me_args; i++)
      if (sc->me_args[i].name && !strcmp(sc->me_args[i].name, "raceLost") && sc->me_args[i].v) lost = true;
  }
  snprintf(sub, sizeof sub, lost && option == 1 ? "option.%d.selectedBad" : "option.%d.selected", option + 1);
  if (fmt(sub, extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
  if (fmt("option.selected", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
  if (F.type == GC_ME_TRASH_TO_TREASURE && option == 0 && fmt("option.1.selected2", extra, n_extra, true, page, sizeof page)) append_page(ch.selected,sizeof ch.selected,page);
  if (F.type == GC_ME_FIELD_TRIP) {
    if (fmt(field_correct ? "correct" : "incorrect", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
    if (fmt(field_correct ? "correctExp" : "incorrectExp", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
  }
  if (F.type == GC_ME_THE_POKEMON_SALESMAN && option == 0) {
    if (fmt("option.1.selectedMessage", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
    if (fmt("option.1.selectedDialogue", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
  }
  void (*cb)(const me_choice*, void*) = F.on_choice;
  void* arg = F.arg;
  if (cb) cb(&ch, arg);
}

static void fill_title(me_args* a)
{
  char t[ME_LABEL_LEN];
  a->title[0] = 0;
  if (fmt("title", NULL, 0, false, t, sizeof t)) snprintf(a->title, sizeof a->title, "%s", t);
}

static void on_move(int idx, void* arg)
{
  (void)arg;
  finish(F.option, F.member, F.pick_row[idx]);
}

static void on_move_back(void* arg);

static void on_trait(int idx, void* arg)
{
  (void)arg;
  finish(F.option, F.member, idx < F.n_trait ? F.trait[idx] : 0);
}

/* the natures (25 of them) or the abilities a member can have, "(new)" where the dex does not have it yet */
static void open_traits(void)
{
  static me_args a; /* static: 25 rows are too much for the 3DS stack; me_screen_open copies it */
  memset(&a, 0, sizeof a);
  fill_title(&a);
  const bool nature = F.run->screen.option_pick[F.option] == GC_PICK_MEMBER_NATURE;
  snprintf(a.query, sizeof a.query, nature ? "Choose a Nature." : "Choose an Ability.");
  F.n_trait = 0;
  if (nature) {
    for (int i = 0; i < 25 && a.n < ME_MAX_OPTIONS; i++) {
      char nm[40];
      i18n_t(nm, sizeof nm, gc_nature_key(i), NULL); /* the key is already "nature:<name>" */
      snprintf(a.options[a.n].label, ME_LABEL_LEN, "%s%s", nm, gc_training_nature_new(F.run, F.member, i) ? " (new)" : "");
      a.options[a.n].ok = true;
      F.trait[F.n_trait++] = i;
      a.n++;
    }
  } else {
    int slots[3];
    bool fresh[3];
    const int n = gc_training_abilities(F.run, F.member, slots, fresh);
    for (int i = 0; i < n; i++) {
      snprintf(a.options[a.n].label, ME_LABEL_LEN, "%s%s", i18n_ability_name(gc_ability_of_slot(F.run, F.member, slots[i])), fresh[i] ? " (new)" : "");
      a.options[a.n].ok = true;
      F.trait[F.n_trait++] = slots[i];
      a.n++;
    }
  }
  a.can_back = true;
  a.on_pick = on_trait;
  a.on_back = on_move_back;
  if (!me_screen_open(&a)) finish(F.option, F.member, 0);
}

static void on_move_back(void* arg)
{
  (void)arg;
  open_members();
}

static void open_moves(void)
{
  const gc_mon* m = &F.run->party[F.member];
  static me_args a; /* static: 25 rows are too much for the 3DS stack; me_screen_open copies it */
  memset(&a, 0, sizeof a);
  fill_title(&a);
  snprintf(a.query, sizeof a.query, "Choose a move.");
  for (int i = 0; i < m->b.n_moves && a.n < 4; i++) {
    if (m->b.moves[i] <= 0) continue;
    snprintf(a.options[a.n].label, ME_LABEL_LEN, "%s", i18n_move_name(m->b.moves[i]));
    a.options[a.n].ok = true;
    F.pick_row[a.n] = i;
    a.n++;
  }
  a.can_back = true;
  a.on_pick = on_move;
  a.on_back = on_move_back;
  if (!me_screen_open(&a)) finish(F.option, F.member, 0);
}

static void on_trade_offer(int idx, void* arg)
{
  (void)arg;
  finish(F.option, F.member, idx);
}

static void open_trade_offers(void)
{
  static gc_mon offers[3];
  static me_args a;
  memset(&a, 0, sizeof a);
  fill_title(&a);
  snprintf(a.query, sizeof a.query, "Choose a trade offer.");
  /* The core generator uses a temporary offset stream and restores the run RNG. It returns copies. */
  if (!gc_gts_trade_options((gc_run*)F.run, F.member, offers)) {
    open_members();
    return;
  }
  for (int i = 0; i < 3; i++) {
    const gc_mon* m = &offers[i];
    snprintf(a.options[i].label, ME_LABEL_LEN, "%s  Lv.%u%s", i18n_species_name(m->b.species),
             (unsigned)m->b.level, m->b.shiny ? " (Shiny)" : "");
    snprintf(a.options[i].tooltip, ME_TOOLTIP_LEN, "%s\nNature: %s\nAbility: %s", a.options[i].label,
             i18n_t(a.description, sizeof a.description, gc_nature_key(m->b.nature), NULL), i18n_ability_name(m->b.ability));
    a.options[i].ok = true;
  }
  a.description[0] = 0;
  a.n = 3;
  a.can_back = true;
  a.on_pick = on_trade_offer;
  a.on_back = on_move_back;
  me_screen_open(&a);
}

static void on_item(int idx, void* arg)
{
  (void)arg;
  const int item = F.pick_row[idx];
  if (item < 0) {
    F.item_page += item == -1 ? -1 : 1;
    open_items();
  } else finish(F.option, F.member, item);
}

static void open_items(void)
{
  static int held[GC_MAX_ITEMS];
  const int n = gc_gts_held_items(F.run, F.member, held);
  static me_args a;
  memset(&a, 0, sizeof a);
  fill_title(&a);
  snprintf(a.query, sizeof a.query, "Choose an item to trade.");
  /* Keep all inventory entries reachable without growing the 25-row screen on the 3DS. */
  const int page_size = ME_MAX_OPTIONS - 2;
  for (int i = F.item_page * page_size; i < n && i < (F.item_page + 1) * page_size; i++) {
    const gc_item* it = &F.run->items[held[i]];
    const gm_item_ui* display = gm_item_ui_find(it->key, it->arg);
    me_option* o = &a.options[a.n];
    snprintf(o->label, sizeof o->label, "%s x%u", display ? display->name : it->key, (unsigned)it->stack);
    if (display) snprintf(o->tooltip, sizeof o->tooltip, "%s", display->desc);
    o->ok = true;
    F.pick_row[a.n++] = i;
  }
  if (F.item_page > 0) {
    snprintf(a.options[a.n].label, ME_LABEL_LEN, "Previous page");
    a.options[a.n].ok = true;
    F.pick_row[a.n++] = -1;
  }
  if ((F.item_page + 1) * page_size < n) {
    snprintf(a.options[a.n].label, ME_LABEL_LEN, "Next page");
    a.options[a.n].ok = true;
    F.pick_row[a.n++] = -2;
  }
  a.can_back = true;
  a.on_pick = on_item;
  a.on_back = on_move_back;
  if (!me_screen_open(&a)) open_members();
}

/* the list index -> the party slot: the screen lists the party in its order (the order the player sees) */
static int slot_of(int idx)
{
  return idx >= 0 && idx < F.run->n_party ? F.run->party_order[idx] : 0;
}

static void on_member(int idx, void* arg)
{
  (void)arg;
  F.member = slot_of(idx);
  const int pick = F.run->screen.option_pick[F.option];
  if (pick == GC_PICK_MEMBER_MOVE)
    open_moves();
  else if (pick == GC_PICK_MEMBER_NATURE || pick == GC_PICK_MEMBER_ABILITY)
    open_traits();
  else if (pick == GC_PICK_MEMBER_TRADE)
    open_trade_offers();
  else if (pick == GC_PICK_MEMBER_ITEM) {
    F.item_page = 0;
    open_items();
  }
  else
    finish(F.option, F.member, 0);
}

static void on_member_back(void* arg)
{
  (void)arg;
  open_question(F.option);
}

static void open_members(void)
{
  static me_args a; /* static: 25 rows are too much for the 3DS stack; me_screen_open copies it */
  memset(&a, 0, sizeof a);
  fill_title(&a);
  snprintf(a.query, sizeof a.query, "Choose a Pokemon.");
  for (int i = 0; i < F.run->n_party && a.n < ME_MAX_OPTIONS; i++) {
    const int slot = slot_of(i);
    const gc_mon* m = &F.run->party[slot];
    snprintf(a.options[a.n].label, ME_LABEL_LEN, "%s  Lv.%d  %d/%d", i18n_species_name(m->b.species), (int)m->b.level, (int)m->b.hp,
             (int)m->b.stats[0]);
    a.options[a.n].ok = gc_mystery_member_ok(F.run, F.option, slot);
    a.n++;
  }
  a.can_back = true;
  a.on_pick = on_member;
  a.on_back = on_member_back;
  if (!me_screen_open(&a)) finish(F.option, 0, 0);
}

static void on_option(int idx, void* arg)
{
  (void)arg;
  F.option = idx;
  if (F.run->screen.option_pick[idx])
    open_members();
  else
    finish(idx, 0, 0);
}

static void open_question(int cursor)
{
  const gc_screen* s = gc_run_screen(F.run);
  static me_args a; /* static: 25 rows are too much for the 3DS stack; me_screen_open copies it */
  memset(&a, 0, sizeof a);
  fill_title(&a);
  bool salesman_shiny = false;
  if (F.type == GC_ME_THE_POKEMON_SALESMAN)
    for (int i = 0; i < s->n_me_args; i++)
      if (!strcmp(s->me_args[i].name, "salesmanShiny")) salesman_shiny = s->me_args[i].v != 0;
  fmt(salesman_shiny ? "descriptionShiny" : "description", NULL, 0, false, a.description, sizeof a.description);
  fmt("query", NULL, 0, false, a.query, sizeof a.query);
  a.cursor = cursor;
  for (int i = 0; i < s->n_options && i < ME_MAX_OPTIONS; i++) {
    char sub[48];
    me_option* o = &a.options[a.n++];
    o->ok = s->option_ok[i];
    snprintf(sub, sizeof sub, "option.%d.label%s", i + 1, o->ok ? "" : "Disabled");
    if (!fmt(sub, NULL, 0, false, o->label, sizeof o->label)) {
      snprintf(sub, sizeof sub, "option.%d.label", i + 1);
      if (!fmt(sub, NULL, 0, false, o->label, sizeof o->label)) snprintf(o->label, sizeof o->label, "Option %d", i + 1);
    }
    snprintf(sub, sizeof sub, "option.%d.tooltip%s", i + 1, o->ok ? "" : "Disabled");
    if (salesman_shiny && i == 0) snprintf(sub, sizeof sub, "option.1.tooltipShiny");
    if (!fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip)) {
      /* Both names occur in pinned locales: Lost at Sea uses tooltipDisabled, while Snorlax,
       * Fight or Flight, Part Timer and others use disabledTooltip. */
      snprintf(sub, sizeof sub, "option.%d.disabledTooltip", i + 1);
      if (o->ok || !fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip)) {
        snprintf(sub, sizeof sub, "option.%d.tooltip", i + 1);
        fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip);
      }
    }
  }
  if (F.type == GC_ME_PILLAR_MEN)
    for (int i = 0; i < 3 && i < s->n_options; i++) snprintf(a.sprites[i], sizeof a.sprites[i], "images/trainer/custom/%s", PILLAR_ID[i]);
  if (F.type != GC_ME_PILLAR_MEN) a.n_intro = me_flow_intro_sprites(F.type, 160, 164, a.intro, ME_MAX_INTRO);
  a.can_back = false;
  a.on_pick = on_option;
  if (!me_screen_open(&a)) finish(0, 0, 0);
}

bool me_flow_open(const gc_run* run, void (*on_choice)(const me_choice* c, void* arg), void* arg)
{
  const gc_screen* s = gc_run_screen(run);
  if (s->kind != GC_SCREEN_MYSTERY || s->n_options < 1) return false;
  memset(&F, 0, sizeof F);
  F.run = run;
  F.type = s->mystery_type;
  F.on_choice = on_choice;
  F.arg = arg;
  open_question(0);
  return true;
}
