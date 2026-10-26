/* A Mystery Encounter's texts, question and picks (see me_flow.h). */
#include "me_flow.h"

#include <stdio.h>
#include <string.h>

#include "battle_events.h"
#include "i18n.h"
#include "me_screen.h"

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
  void (*on_choice)(const me_choice* c, void* arg);
  void* arg;
} F;

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

/* ------------------------------------------------------------------ the picks */

static void open_question(int cursor);
static void open_members(void);

static void finish(int option, int b, int c)
{
  me_choice ch = {.option = option, .b = b, .c = c};
  pr_msg_arg extra[1];
  int n_extra = 0;
  if (F.run && b >= 0 && b < F.run->n_party && F.run->screen.option_pick[option])
    extra[n_extra++] = pr_arg_pokemon_name(NULL, "selectedPokemon", &F.run->party[b].b);
  char sub[48], page[ME_TEXT_LEN * 2];
  snprintf(sub, sizeof sub, "option.%d.selected", option + 1);
  if (fmt(sub, extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
  if (fmt("option.selected", extra, n_extra, true, page, sizeof page)) append_page(ch.selected, sizeof ch.selected, page);
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
  finish(F.option, F.member, idx);
}

static void on_move_back(void* arg)
{
  (void)arg;
  open_members();
}

static void open_moves(void)
{
  const gc_mon* m = &F.run->party[F.member];
  me_args a;
  memset(&a, 0, sizeof a);
  fill_title(&a);
  snprintf(a.query, sizeof a.query, "Choose a move.");
  for (int i = 0; i < m->b.n_moves && a.n < 4; i++) {
    if (m->b.moves[i] <= 0) continue;
    snprintf(a.options[a.n].label, ME_LABEL_LEN, "%s", i18n_move_name(m->b.moves[i]));
    a.options[a.n].ok = true;
    a.n++;
  }
  a.can_back = true;
  a.on_pick = on_move;
  a.on_back = on_move_back;
  if (!me_screen_open(&a)) finish(F.option, F.member, 0);
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
  if (F.run->screen.option_pick[F.option] == GC_PICK_MEMBER_MOVE)
    open_moves();
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
  me_args a;
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
  me_args a;
  memset(&a, 0, sizeof a);
  fill_title(&a);
  fmt("description", NULL, 0, false, a.description, sizeof a.description);
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
    if (!fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip)) {
      snprintf(sub, sizeof sub, "option.%d.tooltip", i + 1);
      fmt(sub, NULL, 0, false, o->tooltip, sizeof o->tooltip);
    }
  }
  if (F.type == GC_ME_PILLAR_MEN)
    for (int i = 0; i < 3 && i < s->n_options; i++) snprintf(a.sprites[i], sizeof a.sprites[i], "images/trainer/custom/%s", PILLAR_ID[i]);
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
