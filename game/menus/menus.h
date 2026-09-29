/*
 * Menu screens of the PokeRogue 3DS port (game/menus/, the second screens
 * thread): party, summary, reward select, starter select, learn move and the
 * next-biome choice. Ports of upstream's src/ui/handlers/ (party-ui-handler.ts and co.).
 *
 * The screens don't read battle-core or game-core state. The caller fills the
 * plain display structs below and gets choices back through callbacks, like
 * upstream's selectCallback. Each screen is a single instance (upstream's
 * handlers are too): open it, feed it buttons, update and draw it each frame
 * until its callback says it closed.
 *
 * Drawing uses upstream's 320x180 canvas coordinates; see mui.h.
 */
#ifndef MENUS_H
#define MENUS_H

#include <stdbool.h>
#include <stdint.h>

#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------ display data */

/* upstream StatusEffect */
enum { MS_NONE, MS_POISON, MS_TOXIC, MS_PARALYSIS, MS_SLEEP, MS_FREEZE, MS_BURN, MS_FAINT };
/* upstream Gender */
enum { MG_GENDERLESS = -1, MG_MALE = 0, MG_FEMALE = 1 };

#define MENU_MAX_ITEMS 24

typedef struct {
  char name[40];          /* modifier type name, e.g. "Leftovers" */
  const char* icon;       /* frame in the "items" atlas, e.g. "leftovers" (may be NULL) */
  uint16_t count, max;    /* stack count and its maximum */
  bool transferable;
} menu_held_item;

typedef struct {
  uint16_t id;            /* MoveId */
  uint8_t pp_used, pp_max;
} menu_move;

/* One party member, as the menus show it. */
typedef struct {
  uint16_t species;       /* SpeciesId */
  uint8_t form;           /* formIndex */
  int8_t gender;          /* MG_* */
  bool shiny;
  uint8_t variant;        /* 0..2 */
  bool fusion;
  uint16_t fusion_species;
  uint8_t fusion_form;
  int8_t fusion_gender;
  bool fusion_shiny;
  uint8_t fusion_variant;

  char name[40];          /* getNameToRender(): nickname or species name */
  uint16_t level;
  uint32_t hp, max_hp;
  uint8_t status;         /* MS_* (the non-volatile status; 0 = none) */

  menu_move moves[4];
  uint8_t nmoves;
  menu_held_item items[MENU_MAX_ITEMS];
  uint8_t nitems;

  bool has_evolutions;    /* shows "Pause Evolution" (either fused species evolving counts) */
  bool pause_evolutions;
  bool allowed_in_battle; /* challenge rules (FilterChallengeLegal); true outside challenges */
  uint8_t tm_state;       /* TM_MODIFIER mode: 0 not able, 1 able, 2 learned */
  /* REMEMBER_MOVE_MODIFIER mode: the relearnable moves (getLearnableLevelMoves) */
  uint16_t relearn[32];
  uint8_t relearn_level[32]; /* level learned, 0 = TM/egg source */
  uint8_t nrelearn;
} menu_mon;

/* ------------------------------------------------------------ party screen */

/* upstream PartyUiMode */
typedef enum {
  PARTY_SWITCH, PARTY_FAINT_SWITCH, PARTY_POST_BATTLE_SWITCH, PARTY_REVIVAL_BLESSING, PARTY_MODIFIER,
  PARTY_MOVE_MODIFIER, PARTY_TM_MODIFIER, PARTY_REMEMBER_MOVE_MODIFIER, PARTY_MODIFIER_TRANSFER, PARTY_SPLICE,
  PARTY_RELEASE, PARTY_CHECK, PARTY_SELECT, PARTY_DISCARD,
} party_mode;

/* upstream PartyOption */
enum {
  PO_CANCEL = -1, PO_SEND_OUT, PO_PASS_BATON, PO_REVIVE, PO_APPLY, PO_TEACH, PO_TRANSFER, PO_SUMMARY, PO_POKEDEX,
  PO_UNPAUSE_EVOLUTION, PO_SPLICE, PO_UNSPLICE, PO_RELEASE, PO_RENAME, PO_SELECT, PO_DISCARD,
  PO_SCROLL_UP = 1000, PO_SCROLL_DOWN = 1001, PO_FORM_CHANGE_ITEM = 2000, PO_MOVE_1 = 3000, PO_MOVE_2, PO_MOVE_3,
  PO_MOVE_4, PO_ALL = 4000,
};

/* A filter returns NULL when the Pokemon (or its move) may be picked, else the
 * message to show (upstream PokemonSelectFilter / PokemonMoveSelectFilter). */
typedef const char* (*party_filter)(void* ctx, int slot);
typedef const char* (*party_move_filter)(void* ctx, int slot, int move_index);

typedef struct party_events {
  /* A pick (upstream PartySelectCallback). slot 0..5, or 6 with PO_CANCEL
   * when the player backs out. For PO_MOVE_1..4 the move index is
   * option - PO_MOVE_1; in REMEMBER_MOVE_MODIFIER option is the index into
   * relearn[]. The screen stays open; the caller closes it (party_close) or
   * leaves it open for another pick, like upstream. */
  void (*on_select)(void* ctx, int slot, int option);
  /* MODIFIER_TRANSFER: move `count` of held item `item` from `from` to `to`
   * (upstream PartyModifierTransferSelectCallback). The caller updates the
   * party data and calls party_refresh. */
  void (*on_transfer)(void* ctx, int from, int item, int count, int to);
  /* DISCARD: throw away `count` of held item `item` (-1 = all items). */
  void (*on_discard)(void* ctx, int slot, int item, int count);
  /* RELEASE confirmed (any mode that offers it): remove the member. The caller
   * removes it from its party, then calls party_refresh. */
  void (*on_release)(void* ctx, int slot);
  /* Pause/unpause evolutions was toggled. */
  void (*on_pause_evolution)(void* ctx, int slot, bool paused);
  /* SUMMARY picked: the caller shows the summary screen and calls
   * party_resume when it closes (menus' own summary_open does). NULL = the
   * option is hidden. */
  void (*on_summary)(void* ctx, int slot);
  void* ctx;
} party_events;

typedef struct {
  party_mode mode;
  int field_index;          /* which active Pokemon is switching (SWITCH) */
  int battler_count;        /* 1 singles, 2 doubles (getBattlerCount) */
  bool double_battle;
  int max_level;            /* getMaxExpLevel(): levels at or above it show red */
  party_filter filter;      /* NULL = FilterAll */
  party_move_filter move_filter; /* NULL = FilterAllMoves */
  uint16_t tm_move;         /* TM_MODIFIER: the move (for the "Able"/"Learned" labels see menu_mon.tm_state) */
  bool show_move_pp;        /* MOVE_MODIFIER: "Tackle 35/35" */
  bool baton_item;          /* the switching Pokemon holds a Baton (SwitchEffectTransferModifier) */
  bool baton_pass_move;     /* FAINT_SWITCH after a successful Baton Pass */
  bool prefer_baton_pass;   /* settings.general.preferBatonPass */
  bool hardcore;            /* HARDCORE challenge: fainted members only offer release */
  bool in_reward_phase;     /* CHECK opened from the reward screen: offers Release (and Unsplice) */
} party_args;

void party_open(const party_args* args, const menu_mon* party, int n, const party_events* ev);
/* The caller changed the party (release, item transfer, heal...): redraw from it. */
void party_refresh(const menu_mon* party, int n);
/* Show a message in the party's box (upstream partyUiHandler.showText), e.g.
 * "It won't have any effect." after an APPLY; prompt waits for A. */
void party_show_text(const char* text, bool prompt);
void party_resume(void);   /* back from a screen opened on top (summary) */
void party_close(void);
bool party_active(void);
bool party_input(mui_button b);
void party_update(float dt_ms);
void party_draw(void);

/* ------------------------------------------------------------ icons */

/* Draws a Pokemon's party icon (upstream addPokemonIcon; fusions show the top
 * half of one icon over the bottom half of the other). ox/oy as in upstream:
 * x is the icon's centre when ox = 0.5; oy 0.5 centres it vertically. */
void menu_draw_icon(const menu_mon* m, float x, float y, float ox, float oy);

#ifdef __cplusplus
}
#endif
#endif
