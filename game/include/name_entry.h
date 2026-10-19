/*
 * name_entry: a text entry screen for the keyboard-less 3DS buttons. Upstream's Rename Run (src/ui/handlers/rename-run-ui-handler.ts, a
 * FormModalUiHandler over a browser text input) has no 3DS counterpart that draws in this app's windows and works in the emulator, so the
 * letters are picked from a grid with the D-pad.
 *
 * Top screen: upstream's form modal (160 wide there, 220 here for 20 characters): the title, the field's label and the text in a box.
 * Bottom screen: the keys, a row of Shift / Space / Delete, and the two buttons upstream's modal has (the owner's "ok" and "cancel" words).
 *
 * Buttons: the D-pad moves over the keys, A presses one. B deletes the last character and, with nothing typed, cancels. START presses the ok
 * button. L / R switch Shift (off, the next letter only, all letters), X types a space, Y deletes (the same as the keys they stand for).
 * The text is at most `max_chars` characters (a key that does not fit gives the error sound) and, as upstream's submit does, is trimmed
 * (sanitizeInputs) before it is handed over.
 *
 * The keys: digits, letters (Shift makes them capitals), the apostrophe, comma, full stop, hyphen, the accented vowels of the Romance
 * languages and a few marks. The characters are UTF-8; Shift's capitals for the accented ones are the Latin-1 ones.
 */
#ifndef GM_NAME_ENTRY_H
#define GM_NAME_ENTRY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NE_MAX_BYTES 128 /* the text's buffer, NUL included; max_chars is at most NE_MAX_BYTES / 4 - 1 */

typedef struct {
  /* i18n keys of the words: upstream's "menu:renameRun", "menu:runName", "menu:rename" and "menu:cancel" for Rename Run */
  const char* title_key;
  const char* label_key;
  const char* ok_key;
  const char* cancel_key;
  int max_chars;
  /* The screen is done. `text` is the trimmed text, "" when nothing (but spaces) was typed; NULL when it was cancelled (the cancel button, or B
   * with nothing typed). The screen has not been replaced: the callback does that (ui_set), and no input is taken after it was called. */
  void (*on_done)(const char* text, void* arg);
  void* arg;
} name_entry_args;

/* Makes the entry the screen (ui_set, with a fade). */
void name_entry_open(const name_entry_args* a);

#ifdef __cplusplus
}
#endif
#endif
