/*
 * me_screen: a Mystery Encounter's question (upstream's MysteryEncounterUiHandler, src/ui/handlers/mystery-encounter-ui-handler.ts) and
 * the lists the player picks from before an option runs.
 *
 * Top screen: the encounter's title, its description and the question. Bottom screen: the options as rows (a disabled one is dimmed and
 * cannot be taken) and, under them, the tooltip of the highlighted one with upstream's colouring: "(+) ..." green and "(-) ..." blue up to
 * the next "(", and the BBCode @[MONEY]{..} / @[TOOLTIP_TITLE]{..} / @[SUMMARY_GREEN]{..} / @[SUMMARY_BLUE]{..} runs.
 *
 * The same screen lists a party (to pick the Pokemon an option uses) and a Pokemon's moves; those lists have a back button, the question
 * does not (upstream cannot back out of an encounter). UP/DOWN move, A takes the row, B backs out where allowed, a tap on a row moves the
 * cursor to it and takes it. Pure presentation: it keeps copies of its texts and calls back with the index; it changes no game state.
 */
#ifndef GM_ME_SCREEN_H
#define GM_ME_SCREEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ME_MAX_OPTIONS 8
#define ME_LABEL_LEN 96
#define ME_TOOLTIP_LEN 320
#define ME_TEXT_LEN 640
#define ME_MAX_SPRITES 3
#define ME_SPRITE_LEN 64

typedef struct {
  char label[ME_LABEL_LEN];
  char tooltip[ME_TOOLTIP_LEN]; /* with BBCode and (+)/(-) as the locale has them */
  bool ok;                      /* false: shown dimmed, A plays the error sound */
} me_option;

#define ME_MAX_INTRO 12
/* One of the encounter's intro sprites (upstream's MysteryEncounterIntroVisuals): where its feet land on the top screen, drawn in colour. */
typedef struct {
  char key[ME_SPRITE_LEN];   /* image key; with frame "" the image's first animation frame */
  char frame[32];            /* "items" atlas icon name (key is then "items"); "" otherwise */
  float x, y, scale;         /* x centre and y feet on the screen; scale 1 = native */
  float dark;                /* 0..1: how far the sprite is shaded toward black (upstream's tint) */
} me_intro_sprite;

typedef struct {
  char title[ME_LABEL_LEN];
  char description[ME_TEXT_LEN];
  char query[ME_TEXT_LEN];
  me_option options[ME_MAX_OPTIONS];
  char sprites[ME_MAX_SPRITES][ME_SPRITE_LEN]; /* image keys drawn as black silhouettes under the texts (the Pillar Men); "" = none */
  me_intro_sprite intro[ME_MAX_INTRO]; /* the encounter's intro sprites (me_flow_intro_sprites), n_intro of them */
  int n_intro;
  int n;
  int cursor;                         /* where the cursor starts */
  bool can_back;                      /* B calls on_back (and the hint "B: back" shows) */
  void (*on_pick)(int idx, void* arg);/* A on an enabled row */
  void (*on_back)(void* arg);         /* B, when can_back */
  void* arg;
} me_args;

/* Puts the screen over the one below (ui_push, no fade). The screen pops itself before it calls on_pick or on_back, so the callee may
 * open the next one. False (nothing opened) for n < 1 or n > ME_MAX_OPTIONS. */
bool me_screen_open(const me_args* a);

/* ---- BBCode, exposed for the host tests ---- */
enum { ME_K_NORMAL = 0, ME_K_TITLE, ME_K_MONEY, ME_K_GREEN, ME_K_BLUE };
/* `src` without its @[STYLE]{...} wrappers into `plain`, with the colour of every byte of it in `kinds` (same length as plain, both n
 * bytes at most). "(+)..." / "(-)..." to the next "(" are coloured green / blue as upstream's tooltip does (inside no wrapper). Returns
 * the length of plain. */
size_t me_markup(const char* src, char* plain, uint8_t* kinds, size_t n);

#ifdef __cplusplus
}
#endif
#endif
