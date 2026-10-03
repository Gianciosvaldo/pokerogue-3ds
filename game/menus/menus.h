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
  bool has_type;          /* pokemon.getMoveType() differs from the move's own type (Hidden Power, -ate abilities...) */
  int8_t type;            /* PokemonType when has_type */
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

  /* ---- summary screen ---- */
  int8_t types[2];        /* getTypes (no tera, no third type); types[1] = -1 when single-typed */
  uint16_t ability, passive; /* AbilityId (getAbility(true), getPassiveAbility) */
  bool has_passive;
  uint8_t nature;         /* Nature */
  uint32_t stats[6];      /* getStat, in PERMANENT_STATS order (stats[0] unused: hp/max_hp) */
  uint8_t ivs[6];
  uint32_t exp, next_lv_exp; /* exp and getLevelTotalExp(level + 1) - exp (0 at the level cap) */
  float exp_ratio;        /* levelExp / getLevelRelExp(level + 1), 0 at the level cap */
  uint8_t pokeball;       /* PokeballType */
  uint8_t friendship;
  uint8_t luck;           /* getLuck() */
  bool pokerus;
  bool champion;          /* classic wins for both root species (champion ribbon) */
  int8_t tera_type;       /* shown when show_tera (the Terastallize achievement is unlocked) */
  bool show_tera;
  int16_t met_biome;      /* BiomeId, -1 = unknown ("apparently met") */
  uint16_t met_level, met_wave; /* met_wave 0 = unknown */
  /* starter data of the root species */
  uint16_t candy_count;
  uint16_t starter_friendship, friendship_cap; /* getStarterValueFriendshipCap */
  uint32_t candy_colors[2]; /* getStarterColors: candy and overlay tints, 0xRRGGBB */
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

/* ------------------------------------------------------------ summary screen */

/* upstream SummaryUiMode and Page */
typedef enum { SUMMARY_DEFAULT, SUMMARY_LEARN_MOVE } summary_mode;
typedef enum { SUMMARY_PROFILE, SUMMARY_STATS, SUMMARY_MOVES } summary_page;

typedef struct {
  /* DEFAULT: the player backed out (selectCallback(-1)). The screen is
   * closed; go back to the party screen (party_resume) or the message box. */
  void (*on_close)(void* ctx);
  /* DEFAULT: the player swapped two moves of party member `slot`. The screen
   * already shows them swapped; the caller swaps its own moveset. */
  void (*on_move_swap)(void* ctx, int slot, int a, int b);
  /* LEARN_MOVE (moveSelectFunction): 0..3 = forget that move, 4 = give up on
   * the new move. The screen stays open; the caller closes it (summary_close)
   * when the learn-move phase moves on, like upstream. */
  void (*on_move_select)(void* ctx, int cursor);
  void* ctx;
} summary_events;

typedef struct {
  summary_mode mode;
  summary_page page;           /* DEFAULT: the page to open on */
  const menu_mon* party;       /* the member shown is party[index] */
  int n, index;
  bool from_party;             /* opened from the party screen: UP/DOWN go through the party */
  uint16_t new_move;           /* LEARN_MOVE: the move to learn */
  int8_t new_move_type;        /* its getMoveType, -1 = the move's own type */
  const char* trainer_name;    /* OT (loggedInUser.username; NULL = "Unknown") */
  const char* trainer_id;      /* gameData.trainerId as text */
  bool player_female;          /* OT colour */
} summary_args;

void summary_open(const summary_args* args, const summary_events* ev);
void summary_close(void);
bool summary_active(void);
bool summary_input(mui_button b);
void summary_update(float dt_ms);
void summary_draw(void);

/* Helper to parse active unit's Shiny Tier (0 = None, 1 = Yellow, 2 = Blue, 3 = Red) */
int summary_mon_shiny_tier(const menu_mon* m, int* fusion_tier);
const char* summary_shiny_tier_name(int tier);

/* ------------------------------------------------------------ learn move */

/* The learn-move dialog (upstream LearnMovePhase's messages, confirm boxes and
 * the summary in LEARN_MOVE mode). The host shows the text in its message box
 * (battle or evolution scene) and the yes/no box; this runs the sequence. */
typedef void (*menu_cb)(void* ctx);
typedef struct {
  /* ui.showText: text may hold "$" page breaks and upstream's @d{ms} / @s{sound}
   * tags. With prompt, wait for A before calling done. */
  void (*show_text)(void* host, const char* text, bool prompt, menu_cb done, void* done_ctx);
  /* UiMode.CONFIRM (mui_confirm does it) */
  void (*confirm)(void* host, menu_cb yes, menu_cb no, void* cb_ctx);
  /* The answer (game-core's GC_CH_FORGET): the slot the move goes into
   * (0..3; a free slot when the moveset has room), or -1 = not learned. The
   * host changes the moveset, then the dialog shows "learned" and calls on_end. */
  void (*on_result)(void* host, int slot);
  void (*on_end)(void* host);  /* the phase ends (after the last message) */
  void* host;
} learn_move_host;

typedef struct {
  const menu_mon* party;  /* the summary pages through nothing else: only party[index] is shown */
  int n, index;
  uint16_t move;
  const char* pokemon_name; /* getPokemonNameWithAffix (party members: the name) */
  bool level_move_confirmation; /* settings.general.levelMoveConfirmation (default true) */
  const char* trainer_name;
  const char* trainer_id;
  bool player_female;
} learn_move_args;

void learn_move_start(const learn_move_args* args, const learn_move_host* host);

/* ------------------------------------------------------------ sprites */

/* A Pokemon's battle sprite (front or back; upstream getSpriteKey: forms,
 * female sprites, shiny and variant colours) and its animation. Fusions use
 * the base species' sprite: the fusion palette swap isn't ported yet. */
#define MENU_SPRITE_MAX_FRAMES 400
typedef struct {
  rr_atlas* atlas;
  const rr_frame* frames[MENU_SPRITE_MAX_FRAMES]; /* 0001.png, 0002.png... in order */
  int n;
} menu_sprite;
bool menu_sprite_load(menu_sprite* s, const menu_mon* m, bool back);
void menu_sprite_free(menu_sprite* s);
/* The frame `ms` into the animation (upstream: 10 frames a second, looping). */
const rr_frame* menu_sprite_frame(const menu_sprite* s, uint32_t ms);

/* ------------------------------------------------------------ names */

const char* menu_move_name(uint16_t id);
/* The Pokemon's cry sound key (without "audio/"), e.g. "cry/257". */
void menu_cry_key(const menu_mon* m, char* out, size_t n);

/* ------------------------------------------------------------ icons */

/* Draws a Pokemon's party icon (upstream addPokemonIcon; fusions show the top
 * half of one icon over the bottom half of the other). ox/oy as in upstream:
 * x is the icon's centre when ox = 0.5; oy 0.5 centres it vertically. */
void menu_draw_icon(const menu_mon* m, float x, float y, float ox, float oy);

/* ------------------------------------------------------------ biome map choice */

#define BIOME_MAP_MAX_OPTIONS 8

typedef struct {
  /* Option selected (0..n_biomes-1). Selection is mandatory (no cancel). */
  void (*on_select)(void* ctx, int option);
  void* ctx;
} biome_map_events;

typedef struct {
  const uint8_t* biomes; /* BiomeId list (from gc_screen.biomes) */
  int n_biomes;
} biome_map_args;

void biome_map_open(const biome_map_args* args, const biome_map_events* ev);
void biome_map_close(void);
bool biome_map_active(void);
bool biome_map_input(mui_button b);
void biome_map_update(float dt_ms);
void biome_map_draw(void);

/* ------------------------------------------------------------ shop & reward select UI */

#include "shop_ui.h"

#ifdef __cplusplus
}
#endif
#endif
