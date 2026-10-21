/* Localized move labels for party, summary, starter, TM and learn menus. */
#include <stdio.h>
#include "menus.h"
#include "pr_data.h"

const char* menu_move_name(uint16_t id) {
  extern const char* const menu_move_keys[];
  extern const int menu_move_key_count;
  if (id == 0) return pr_moves[0].name;
  if (id < PR_MOVE_COUNT && id < menu_move_key_count && pr_moves[id].id == id && menu_move_keys[id]) {
    char key[96];
    snprintf(key, sizeof key, "move:%s.name", menu_move_keys[id]);
    /* mui_t returns its input on a missing locale; keep that fallback alive too. */
    static char labels[16][96];
    static unsigned next;
    char* label = labels[next++ % 16];
    snprintf(label, sizeof labels[0], "%s", mui_t(key));
    return label;
  }
  return "???";
}

